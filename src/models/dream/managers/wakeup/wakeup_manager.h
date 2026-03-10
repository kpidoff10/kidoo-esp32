#ifndef WAKEUP_MANAGER_H
#define WAKEUP_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "common/managers/rtc/rtc_manager.h"
#include "common/managers/sd/sd_manager.h"
#include "common/managers/led/led_manager.h"
#include "../schedule_state.h"
#include "../schedule_utils.h"

/**
 * Gestionnaire automatique du wake-up pour le modèle Dream
 * 
 * Ce manager vérifie périodiquement l'heure via le RTC et déclenche
 * automatiquement l'effet wake-up selon la configuration sauvegardée sur la SD.
 * 
 * Fonctionnalités:
 * - Charge la configuration depuis la SD
 * - Parse le weekdaySchedule (JSON)
 * - Vérifie l'heure toutes les minutes
 * - Déclenche l'effet wake-up automatiquement 15 minutes avant l'heure configurée
 * - Gère les transitions de fade-in (1 minute) avec transition de couleur
 * - Transition de la couleur de coucher vers la couleur de réveil
 * - Brightness part de la valeur actuelle vers la brightness cible (ne repart pas de 0)
 */

// Structure pour un horaire de réveil
struct WakeupSchedule {
  uint8_t hour;      // Heure (0-23)
  uint8_t minute;    // Minute (0-59)
  bool activated;    // Si true, le jour est activé
};

// Structure pour la configuration wake-up complète
struct WakeupConfig {
  uint8_t colorR;
  uint8_t colorG;
  uint8_t colorB;
  uint8_t brightness;  // 0-100
  WakeupSchedule schedules[7]; // Un schedule par jour (0=monday, 6=sunday)
};

class WakeupManager {
public:
  /**
   * Initialiser le gestionnaire wake-up
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
   * Vérifie immédiatement si c'est l'heure de déclencher le wake-up
   * @return true si la configuration a été rechargée, false sinon
   */
  static bool reloadConfig();
  
  /**
   * Vérifier immédiatement si c'est l'heure de déclencher le wake-up
   * (sans attendre la prochaine vérification périodique)
   */
  static void checkNow();
  
  /**
   * Vérifier si le wake-up est activé pour aujourd'hui
   * @return true si activé, false sinon
   */
  static bool isWakeupEnabled();
  
  /**
   * Obtenir la configuration actuelle
   * @return Structure WakeupConfig avec la configuration
   */
  static WakeupConfig getConfig();
  
  /**
   * Vérifier si le wake-up est actuellement actif (en cours)
   * @return true si le wake-up est actif, false sinon
   */
  static bool isWakeupActive();

  /**
   * Arrêter le wake-up manuellement (via PubNub)
   */
  static void stopWakeupManually();

private:
  // État partagé (variables communes entre BedtimeManager et WakeupManager)
  static ScheduleState s_state;

  // Configuration
  static WakeupConfig config;
  static WakeupConfig lastConfig;  // Sauvegarde de la dernière config pour détecter les changements

  // Variables uniques à WakeupManager (transition couleur/brightness)
  static uint8_t startColorR;
  static uint8_t startColorG;
  static uint8_t startColorB;
  
  // Brightness de départ (brightness actuelle au moment du déclenchement)
  static uint8_t startBrightness;
  
  // Dernière couleur appliquée (pour éviter les appels répétés)
  static uint8_t lastColorR;
  static uint8_t lastColorG;
  static uint8_t lastColorB;
  static uint8_t lastBrightness;
  
  // Fonctions privées
  static void parseWeekdaySchedule(const char* jsonStr);
  static void checkWakeupTrigger();
  static void updateCheckingState();  // Vérifier si la routine est activée pour aujourd'hui et mettre à jour checkingEnabled
  static bool configChanged();  // Comparer la config actuelle avec lastConfig
  static unsigned long calculateNextCheckInterval();  // Calculer le prochain intervalle de vérification basé sur la distance jusqu'à l'heure de déclenchement
  static void startWakeup();
  static void updateFadeIn();
  static void updateFadeOut();
  static void stopWakeup();
  static void loadBedtimeColor(); // Charger la couleur de coucher depuis la config bedtime
};

#endif // WAKEUP_MANAGER_H
