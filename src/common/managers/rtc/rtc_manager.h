#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H

#include <Arduino.h>

/**
 * Gestionnaire RTC (multi-puces)
 *
 * - Par défaut : DS3231 (I2C 0x68) — Dream, Sound, etc.
 * - Si KIDOO_RTC_PCF85063 (config modèle) : PCF85063 (ex. Waveshare AMOLED, I2C 0x51)
 *
 * Fonctionnalités communes : lecture/écriture date/heure, sync NTP, timezone.
 * Température : uniquement DS3231 ; PCF85063 retourne 0.
 */

// Structure pour représenter une date/heure
struct DateTime {
  uint16_t year;    // Année complète (ex: 2024)
  uint8_t month;    // Mois (1-12)
  uint8_t day;      // Jour (1-31)
  uint8_t hour;     // Heure (0-23)
  uint8_t minute;   // Minute (0-59)
  uint8_t second;   // Seconde (0-59)
  uint8_t dayOfWeek; // Jour de la semaine (1=Lundi, 7=Dimanche)
};

class RTCManager {
public:
  /**
   * Initialiser le gestionnaire RTC
   * @return true si l'initialisation est réussie, false sinon
   */
  static bool init();
  
  /**
   * Vérifier si le RTC est disponible/opérationnel
   * @return true si le RTC est opérationnel, false sinon
   */
  static bool isAvailable();
  
  /**
   * Vérifier si le RTC est initialisé
   * @return true si le RTC est initialisé, false sinon
   */
  static bool isInitialized();
  
  /**
   * Obtenir la date/heure actuelle (UTC stockée dans le RTC)
   * @return Structure DateTime avec la date/heure
   */
  static DateTime getDateTime();

  /**
   * Obtenir la date/heure en heure locale (UTC + offset timezone).
   * Utilise timezoneId chargé en mémoire au démarrage (loadTimezoneFromConfig).
   * Si pas de timezone configurée ou HAS_SD non défini, retourne getDateTime().
   * @return Structure DateTime avec la date/heure locale
   */
  static DateTime getLocalDateTime();

  /**
   * Charger timezoneId depuis config.json (appelé au démarrage).
   * À rappeler après set-timezone ou config sync pour mettre à jour la mémoire.
   */
  static void loadTimezoneFromConfig();

  /**
   * Obtenir le timezoneId actuellement configuré
   * @return Chaîne IANA (ex: "Europe/Paris"), ou vide si pas configurée
   */
  static const char* getTimezoneId();

  /**
   * Mettre à jour le timezoneId en mémoire (après set-timezone ou config sync).
   * @param timezoneId Chaîne IANA (ex: "Europe/Paris"), max 63 caractères
   */
  static void setTimezoneId(const char* timezoneId);
  
  /**
   * Définir la date/heure
   * @param dt Structure DateTime avec la nouvelle date/heure
   * @return true si réussi, false sinon
   */
  static bool setDateTime(const DateTime& dt);
  
  /**
   * Obtenir l'heure formatée (HH:MM:SS)
   * @return String formatée
   */
  static String getTimeString();
  
  /**
   * Obtenir la date formatée (DD/MM/YYYY)
   * @return String formatée
   */
  static String getDateString();
  
  /**
   * Obtenir la date/heure formatée (DD/MM/YYYY HH:MM:SS)
   * @return String formatée
   */
  static String getDateTimeString();
  
  /**
   * Obtenir le timestamp Unix (secondes depuis 01/01/1970).
   * Le RTC stocke toujours UTC (sync NTP avec configTime(0,0)) - valable partout dans le monde.
   * @return Timestamp Unix UTC
   */
  static uint32_t getUnixTime();

  /**
   * Alias de getUnixTime() - retourne le timestamp UTC pour l'auth device.
   * @return Timestamp Unix UTC
   */
  static uint32_t getUnixTimeUTC();
  
  /**
   * Définir l'heure depuis un timestamp Unix
   * @param timestamp Timestamp Unix
   * @return true si réussi, false sinon
   */
  static bool setUnixTime(uint32_t timestamp);
  
  /**
   * Température interne (DS3231 uniquement ; 0 si PCF85063 ou indisponible)
   */
  static float getTemperature();

  /**
   * Perte d'alimentation / horloge arrêtée (OSF DS3231 ou VL PCF85063)
   */
  static bool hasLostPower();
  
  /**
   * Afficher les informations RTC sur Serial
   */
  static void printInfo();
  
  /**
   * Synchroniser l'heure avec un serveur NTP (nécessite WiFi).
   * Stocke UTC/GMT dans le RTC - pas de gestion par pays.
   * @param gmtOffsetSec Décalage en secondes (0 = UTC par défaut)
   * @param daylightOffsetSec Décalage heure d'été (0 par défaut)
   * @return true si la synchronisation a réussi, false sinon
   */
  static bool syncWithNTP(long gmtOffsetSec = 0, int daylightOffsetSec = 0);
  
  /**
   * Vérifier si l'heure semble valide (année >= 2026)
   * @return true si l'heure semble valide, false sinon
   */
  static bool isTimeValid();
  
  /**
   * Vérifier si une synchronisation NTP a été effectuée depuis le boot
   * @return true si déjà synchronisé, false sinon
   */
  static bool hasBeenSynced();
  
  /**
   * Tenter une synchronisation automatique si nécessaire
   * Appelé après connexion WiFi
   * @return true si sync effectuée ou pas nécessaire, false si échec
   */
  static bool autoSyncIfNeeded();

private:
  static bool initialized;
  static bool available;
  static bool ntpSynced;

  // Fonctions utilitaires
  static uint8_t bcdToDec(uint8_t bcd);
  static uint8_t decToBcd(uint8_t dec);
  static uint8_t readRegister(uint8_t reg);
  static void writeRegister(uint8_t reg, uint8_t value);
  static uint8_t calculateDayOfWeek(uint16_t year, uint8_t month, uint8_t day);
  static DateTime unixToDateTime(uint32_t timestamp);
};

#endif // RTC_MANAGER_H
