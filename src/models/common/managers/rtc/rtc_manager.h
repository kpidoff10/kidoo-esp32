#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H

#include <Arduino.h>

/**
 * Gestionnaire RTC DS3231
 * 
 * Ce module gère l'horloge temps réel DS3231 via I2C.
 * Le DS3231 est très précis grâce à sa compensation de température.
 * 
 * Fonctionnalités:
 * - Lecture/écriture de l'heure
 * - Lecture de la température interne
 * - Gestion des alarmes (optionnel)
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
   * Obtenir la date/heure actuelle
   * @return Structure DateTime avec la date/heure
   */
  static DateTime getDateTime();
  
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
   * Obtenir le timestamp Unix (secondes depuis 01/01/1970)
   * @return Timestamp Unix
   */
  static uint32_t getUnixTime();
  
  /**
   * Définir l'heure depuis un timestamp Unix
   * @param timestamp Timestamp Unix
   * @return true si réussi, false sinon
   */
  static bool setUnixTime(uint32_t timestamp);
  
  /**
   * Obtenir la température interne du DS3231 (précision ±3°C)
   * @return Température en degrés Celsius
   */
  static float getTemperature();
  
  /**
   * Vérifier si l'oscillateur s'est arrêté (perte de courant)
   * Indique que l'heure n'est peut-être plus valide
   * @return true si l'oscillateur s'est arrêté, false sinon
   */
  static bool hasLostPower();
  
  /**
   * Afficher les informations RTC sur Serial
   */
  static void printInfo();
  
  /**
   * Synchroniser l'heure avec un serveur NTP (nécessite WiFi)
   * @param gmtOffsetSec Décalage GMT en secondes (ex: 3600 pour GMT+1)
   * @param daylightOffsetSec Décalage heure d'été en secondes (ex: 3600)
   * @return true si la synchronisation a réussi, false sinon
   */
  static bool syncWithNTP(long gmtOffsetSec = 0, int daylightOffsetSec = 0);
  
  /**
   * Synchroniser l'heure avec NTP en utilisant le fuseau horaire français
   * GMT+1 en hiver, GMT+2 en été (détection automatique)
   * @return true si la synchronisation a réussi, false sinon
   */
  static bool syncWithNTPFrance();
  
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
  // Variables statiques
  static bool initialized;
  static bool available;
  static bool ntpSynced;  // Flag pour éviter les syncs multiples
  
  // Adresse I2C du DS3231
  static const uint8_t DS3231_ADDRESS = 0x68;
  
  // Registres du DS3231
  static const uint8_t REG_SECONDS = 0x00;
  static const uint8_t REG_MINUTES = 0x01;
  static const uint8_t REG_HOURS = 0x02;
  static const uint8_t REG_DAY = 0x03;
  static const uint8_t REG_DATE = 0x04;
  static const uint8_t REG_MONTH = 0x05;
  static const uint8_t REG_YEAR = 0x06;
  static const uint8_t REG_CONTROL = 0x0E;
  static const uint8_t REG_STATUS = 0x0F;
  static const uint8_t REG_TEMP_MSB = 0x11;
  static const uint8_t REG_TEMP_LSB = 0x12;
  
  // Fonctions utilitaires
  static uint8_t bcdToDec(uint8_t bcd);
  static uint8_t decToBcd(uint8_t dec);
  static uint8_t readRegister(uint8_t reg);
  static void writeRegister(uint8_t reg, uint8_t value);
  static uint8_t calculateDayOfWeek(uint16_t year, uint8_t month, uint8_t day);
};

#endif // RTC_MANAGER_H
