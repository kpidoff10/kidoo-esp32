#include "timezone_manager.h"
#include <cstring>
#include <cstdint>

// Helpers DST - optimisés inline
static inline uint8_t _dow(uint16_t y, uint8_t m, uint8_t d) {
  if (m < 3) { m += 12; y--; }
  int k = y % 100, j = y / 100;
  int h = (d + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 - 2 * j) % 7;
  return (uint8_t)(((h + 5) % 7) + 1);  // 1=Lun, 7=Dim
}
static inline uint8_t _daysInMonth(uint16_t y, uint8_t m) {
  static const uint8_t d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return d[m - 1] + (m == 2 && (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)));
}
static uint8_t _lastSunday(uint16_t y, uint8_t m) {
  for (uint8_t d = _daysInMonth(y, m); d >= 1; d--)
    if (_dow(y, m, d) == 7) return d;
  return 1;
}
static uint8_t _nthSunday(uint16_t y, uint8_t m, uint8_t n) {
  for (uint8_t d = 1, c = 0; d <= 31; d++)
    if (_dow(y, m, d) == 7 && ++c == n) return d;
  return 1;
}

/**
 * Obtenir le décalage horaire en secondes pour une timezone IANA
 * @param timezoneId La timezone IANA (ex: "Europe/Paris")
 * @return Le décalage en secondes (positif pour est, négatif pour ouest)
 */
long TimezoneManager::getOffsetSeconds(const char* timezoneId) {
  if (!timezoneId) return 0; // UTC par défaut

  // Europe
  if (strcmp(timezoneId, "UTC") == 0) return 0;
  if (strcmp(timezoneId, "Europe/London") == 0) return 0;       // GMT/BST
  if (strcmp(timezoneId, "Europe/Paris") == 0) return 3600;     // CET/CEST +1
  if (strcmp(timezoneId, "Europe/Berlin") == 0) return 3600;    // CET/CEST +1
  if (strcmp(timezoneId, "Europe/Amsterdam") == 0) return 3600; // CET/CEST +1

  // Amérique du Nord - Ouest
  if (strcmp(timezoneId, "America/Los_Angeles") == 0) return -28800; // PST/PDT -8
  if (strcmp(timezoneId, "America/Denver") == 0) return -25200;      // MST/MDT -7
  if (strcmp(timezoneId, "America/Chicago") == 0) return -21600;     // CST/CDT -6
  if (strcmp(timezoneId, "America/New_York") == 0) return -18000;    // EST/EDT -5
  if (strcmp(timezoneId, "America/Toronto") == 0) return -18000;     // EST/EDT -5

  // Amérique du Sud et Centrale
  if (strcmp(timezoneId, "America/Mexico_City") == 0) return -21600; // CST/CDT -6
  if (strcmp(timezoneId, "America/Sao_Paulo") == 0) return -10800;   // BRT/BRST -3

  // Asie
  if (strcmp(timezoneId, "Asia/Dubai") == 0) return 14400;       // GST +4
  if (strcmp(timezoneId, "Asia/Istanbul") == 0) return 10800;    // EET/EEST +3
  if (strcmp(timezoneId, "Asia/Bangkok") == 0) return 25200;     // ICT +7
  if (strcmp(timezoneId, "Asia/Singapore") == 0) return 28800;   // SGT +8
  if (strcmp(timezoneId, "Asia/Hong_Kong") == 0) return 28800;   // HKT +8
  if (strcmp(timezoneId, "Asia/Shanghai") == 0) return 28800;    // CST +8
  if (strcmp(timezoneId, "Asia/Tokyo") == 0) return 32400;       // JST +9

  // Inde
  if (strcmp(timezoneId, "India/Kolkata") == 0) return 19800;    // IST +5:30

  // Afrique
  if (strcmp(timezoneId, "Africa/Cairo") == 0) return 7200;      // EET/EEST +2
  if (strcmp(timezoneId, "Africa/Johannesburg") == 0) return 7200; // SAST +2
  if (strcmp(timezoneId, "Africa/Lagos") == 0) return 3600;      // WAT +1

  // Océanie
  if (strcmp(timezoneId, "Australia/Sydney") == 0) return 36000;     // AEST/AEDT +10
  if (strcmp(timezoneId, "Australia/Melbourne") == 0) return 36000;  // AEST/AEDT +10
  if (strcmp(timezoneId, "Pacific/Auckland") == 0) return 43200;     // NZST/NZDT +12

  // Par défaut, retourner UTC
  return 0;
}

/**
 * Obtenir l'offset en secondes pour l'heure d'été si applicable
 * @param timezoneId La timezone IANA
 * @return L'offset pour l'heure d'été (généralement 3600 ou 0)
 */
int TimezoneManager::getDaylightOffsetSeconds(const char* timezoneId) {
  if (!timezoneId) return 0;

  // Zones avec changement d'heure d'été/hiver
  // Europe: +1h DST
  if (strcmp(timezoneId, "Europe/London") == 0) return 3600;
  if (strcmp(timezoneId, "Europe/Paris") == 0) return 3600;
  if (strcmp(timezoneId, "Europe/Berlin") == 0) return 3600;
  if (strcmp(timezoneId, "Europe/Amsterdam") == 0) return 3600;

  // Amérique du Nord: +1h DST
  if (strcmp(timezoneId, "America/Los_Angeles") == 0) return 3600;
  if (strcmp(timezoneId, "America/Denver") == 0) return 3600;
  if (strcmp(timezoneId, "America/Chicago") == 0) return 3600;
  if (strcmp(timezoneId, "America/New_York") == 0) return 3600;
  if (strcmp(timezoneId, "America/Toronto") == 0) return 3600;
  if (strcmp(timezoneId, "America/Mexico_City") == 0) return 3600;

  // Asie (pas ou peu de DST)
  if (strcmp(timezoneId, "Asia/Istanbul") == 0) return 3600; // Turquie utilise DST
  if (strcmp(timezoneId, "Africa/Cairo") == 0) return 3600;  // Égypte utilise DST

  // Australie/Océanie: +1h DST (inverse: octobre à avril)
  if (strcmp(timezoneId, "Australia/Sydney") == 0) return 3600;
  if (strcmp(timezoneId, "Australia/Melbourne") == 0) return 3600;
  if (strcmp(timezoneId, "Pacific/Auckland") == 0) return 3600;

  // Zones sans DST
  return 0;
}

// Une seule passe: offset base + DST si applicable
long TimezoneManager::getTotalOffsetSeconds(const char* timezoneId, uint16_t y, uint8_t m, uint8_t d) {
  if (!timezoneId) return 0;
  // EU: dernier dim mars → dernier dim oct
  #define EU_DST() ((m > 3 || (m == 3 && d >= _lastSunday(y,3))) && (m < 10 || (m == 10 && d < _lastSunday(y,10))))
  // US: 2e dim mars → 1er dim nov
  #define US_DST() ((m > 3 || (m == 3 && d >= _nthSunday(y,3,2))) && (m < 11 || (m == 11 && d < _nthSunday(y,11,1))))
  // AU: 1er dim oct → 1er dim avr
  #define AU_DST() ((m == 10 && d >= _nthSunday(y,10,1)) || m >= 11 || m <= 3 || (m == 4 && d < _nthSunday(y,4,1)))
  // NZ: dernier dim sept → 1er dim avr
  #define NZ_DST() ((m == 9 && d >= _lastSunday(y,9)) || m >= 10 || m <= 3 || (m == 4 && d < _nthSunday(y,4,1)))

  if (strcmp(timezoneId, "UTC") == 0) return 0;
  if (strcmp(timezoneId, "Europe/London") == 0) return EU_DST() ? 3600 : 0;
  if (strcmp(timezoneId, "Europe/Paris") == 0) return 3600 + (EU_DST() ? 3600 : 0);
  if (strcmp(timezoneId, "Europe/Berlin") == 0) return 3600 + (EU_DST() ? 3600 : 0);
  if (strcmp(timezoneId, "Europe/Amsterdam") == 0) return 3600 + (EU_DST() ? 3600 : 0);
  if (strcmp(timezoneId, "America/Los_Angeles") == 0) return -28800 + (US_DST() ? 3600 : 0);
  if (strcmp(timezoneId, "America/Denver") == 0) return -25200 + (US_DST() ? 3600 : 0);
  if (strcmp(timezoneId, "America/Chicago") == 0) return -21600 + (US_DST() ? 3600 : 0);
  if (strcmp(timezoneId, "America/New_York") == 0) return -18000 + (US_DST() ? 3600 : 0);
  if (strcmp(timezoneId, "America/Toronto") == 0) return -18000 + (US_DST() ? 3600 : 0);
  if (strcmp(timezoneId, "America/Mexico_City") == 0) return -21600 + (US_DST() ? 3600 : 0);
  if (strcmp(timezoneId, "America/Sao_Paulo") == 0) return -10800;
  if (strcmp(timezoneId, "Asia/Dubai") == 0) return 14400;
  if (strcmp(timezoneId, "Asia/Istanbul") == 0) return 10800 + (EU_DST() ? 3600 : 0);
  if (strcmp(timezoneId, "Asia/Bangkok") == 0) return 25200;
  if (strcmp(timezoneId, "Asia/Singapore") == 0) return 28800;
  if (strcmp(timezoneId, "Asia/Hong_Kong") == 0) return 28800;
  if (strcmp(timezoneId, "Asia/Shanghai") == 0) return 28800;
  if (strcmp(timezoneId, "Asia/Tokyo") == 0) return 32400;
  if (strcmp(timezoneId, "India/Kolkata") == 0) return 19800;
  if (strcmp(timezoneId, "Africa/Cairo") == 0) return 7200 + (EU_DST() ? 3600 : 0);
  if (strcmp(timezoneId, "Africa/Johannesburg") == 0) return 7200;
  if (strcmp(timezoneId, "Africa/Lagos") == 0) return 3600;
  if (strcmp(timezoneId, "Australia/Sydney") == 0) return 36000 + (AU_DST() ? 3600 : 0);
  if (strcmp(timezoneId, "Australia/Melbourne") == 0) return 36000 + (AU_DST() ? 3600 : 0);
  if (strcmp(timezoneId, "Pacific/Auckland") == 0) return 43200 + (NZ_DST() ? 3600 : 0);
  #undef EU_DST
  #undef US_DST
  #undef AU_DST
  #undef NZ_DST
  return 0;
}
