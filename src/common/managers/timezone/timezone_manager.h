#ifndef TIMEZONE_MANAGER_H
#define TIMEZONE_MANAGER_H

#include <Arduino.h>

/**
 * Gestionnaire de timezone IANA
 * Convertit une timezone IANA (ex: "Europe/Paris") en offset UTC en secondes
 */

class TimezoneManager {
public:
  /**
   * Obtenir le décalage horaire en secondes pour une timezone IANA
   * @param timezoneId La timezone IANA (ex: "Europe/Paris")
   * @return Le décalage en secondes (positif pour est, négatif pour ouest)
   */
  static long getOffsetSeconds(const char* timezoneId);

  /**
   * Obtenir l'offset en secondes pour l'heure d'été si applicable
   * @param timezoneId La timezone IANA
   * @return L'offset pour l'heure d'été (généralement 3600 ou 0)
   */
  static int getDaylightOffsetSeconds(const char* timezoneId);

  /**
   * Offset total (base + DST) en une seule passe - optimisé pour getLocalDateTime.
   * @param timezoneId La timezone IANA
   * @param year Année (ex: 2024)
   * @param month Mois 1-12
   * @param day Jour 1-31
   * @return Offset total en secondes (base + DST si applicable)
   */
  static long getTotalOffsetSeconds(const char* timezoneId, uint16_t year, uint8_t month, uint8_t day);
};

#endif // TIMEZONE_MANAGER_H
