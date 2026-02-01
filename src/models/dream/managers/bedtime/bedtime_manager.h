#ifndef BEDTIME_MANAGER_H
#define BEDTIME_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "../../../common/managers/rtc/rtc_manager.h"
#include "../../../common/managers/sd/sd_manager.h"
#include "../../../common/managers/led/led_manager.h"

/**
 * Gestionnaire automatique du bedtime pour le modèle Dream
 * 
 * Ce manager vérifie périodiquement l'heure via le RTC et déclenche
 * automatiquement l'effet bedtime selon la configuration sauvegardée sur la SD.
 * 
 * Fonctionnalités:
 * - Charge la configuration depuis la SD
 * - Parse le weekdaySchedule (JSON)
 * - Vérifie l'heure toutes les minutes
 * - Déclenche l'effet bedtime automatiquement à l'heure configurée
 * - Gère les transitions de fade-in (30 secondes)
 * - Gère l'extinction progressive si timer activé (5 minutes)
 */

// Structure pour un horaire de coucher
struct BedtimeSchedule {
  uint8_t hour;      // Heure (0-23)
  uint8_t minute;    // Minute (0-59)
  bool activated;    // Si true, le jour est activé
};

// Structure pour la configuration bedtime complète
struct BedtimeConfig {
  uint8_t colorR;
  uint8_t colorG;
  uint8_t colorB;
  uint8_t brightness;  // 0-100
  bool allNight;       // Si true, reste allumé toute la nuit
  char effect[32];     // Effet LED ("none", "pulse", "rainbow-soft", "breathe", "nightlight", etc.) - vide ou "none" = couleur fixe
  BedtimeSchedule schedules[7]; // Un schedule par jour (0=monday, 6=sunday)
};

class BedtimeManager {
public:
  /**
   * Initialiser le gestionnaire bedtime
   * @return true si l'initialisation est réussie, false sinon
   */
  static bool init();
  
  /**
   * Mettre à jour le gestionnaire (à appeler périodiquement dans loop())
   * Vérifie l'heure et déclenche les effets si nécessaire
   */
  static void update();
  
  /**
   * Charger la configuration depuis la SD
   * @return true si la configuration a été chargée, false sinon
   */
  static bool loadConfig();
  
  /**
   * Recharger la configuration depuis la SD (utile après une mise à jour)
   * Vérifie immédiatement si c'est l'heure de déclencher le bedtime
   * @return true si la configuration a été rechargée, false sinon
   */
  static bool reloadConfig();
  
  /**
   * Vérifier immédiatement si c'est l'heure de déclencher le bedtime
   * (sans attendre la prochaine vérification périodique)
   */
  static void checkNow();
  
  /**
   * Vérifier si le bedtime est activé pour aujourd'hui
   * @return true si activé, false sinon
   */
  static bool isBedtimeEnabled();
  
  /**
   * Obtenir la configuration actuelle
   * @return Structure BedtimeConfig avec la configuration
   */
  static BedtimeConfig getConfig();
  
  /**
   * Vérifier si le bedtime est actuellement actif (en cours)
   * @return true si le bedtime est actif, false sinon
   */
  static bool isBedtimeActive();

  /**
   * Démarrer le bedtime manuellement (via PubNub)
   * Empêche le déclenchement automatique programmé
   */
  static void startBedtimeManually();

  /**
   * Arrêter le bedtime manuellement (via PubNub)
   */
  static void stopBedtimeManually();

private:
  // Variables statiques
  static bool initialized;
  static BedtimeConfig config;
  static BedtimeConfig lastConfig;  // Sauvegarde de la dernière config pour détecter les changements
  static bool bedtimeActive;
  static bool manuallyStarted; // Flag pour indiquer que le bedtime a été démarré manuellement
  static unsigned long bedtimeStartTime;
  static unsigned long lastCheckTime;
  static uint8_t lastTriggeredHour;
  static uint8_t lastTriggeredMinute;
  static bool checkingEnabled;  // Si true, vérifier toutes les minutes. Si false, attendre le jour suivant
  static uint8_t lastCheckedDay;  // Dernier jour vérifié (1-7, RTC format)
  
  // États de transition
  static bool fadeInActive;
  static bool fadeOutActive;
  static unsigned long fadeStartTime;
  
  // Fonctions privées
  static void parseWeekdaySchedule(const char* jsonStr);
  static uint8_t weekdayToIndex(uint8_t dayOfWeek); // Convertir RTC dayOfWeek (1-7) vers index (0-6)
  static const char* indexToWeekday(uint8_t index); // Convertir index (0-6) vers weekday string
  static void checkBedtimeTrigger();
  static void updateCheckingState();  // Vérifier si la routine est activée pour aujourd'hui et mettre à jour checkingEnabled
  static bool configChanged();  // Comparer la config actuelle avec lastConfig
  static unsigned long calculateNextCheckInterval();  // Calculer le prochain intervalle de vérification basé sur la distance jusqu'à l'heure de déclenchement
  static void startBedtime();
  static void updateFadeIn();
  static void updateFadeOut();
  static void stopBedtime();
};

#endif // BEDTIME_MANAGER_H
