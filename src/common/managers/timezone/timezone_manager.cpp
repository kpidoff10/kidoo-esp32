#include "timezone_manager.h"
#include <cstring>

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
