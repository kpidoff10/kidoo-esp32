#ifndef SCHEDULE_STATE_H
#define SCHEDULE_STATE_H

#include <Arduino.h>

/**
 * État partagé commun entre BedtimeManager et WakeupManager.
 *
 * Regroupe les 14 variables d'état statiques identiques dans les deux managers
 * pour éviter la duplication et faciliter la maintenabilité.
 *
 * Chaque manager (bedtime/wakeup) possède sa propre instance statique : s_state
 */
struct ScheduleState {
  // Initialisation et activation globale
  bool initialized = false;
  bool routineActive = false;  // bedtimeActive / wakeupActive

  // Timing
  unsigned long startTime = 0;              // bedtimeStartTime / wakeupStartTime
  unsigned long lastCheckTime = 0;          // Dernier check RTC
  unsigned long lastFadeUpdateTime = 0;    // Throttling fade (100ms interval)

  // État du déclenchement
  uint8_t lastTriggeredHour = 255;   // 255 = jamais déclenché
  uint8_t lastTriggeredMinute = 255; // 255 = jamais déclenché

  // État du scheduling quotidien
  bool checkingEnabled = false;      // Routine activée pour le jour courant
  uint8_t lastCheckedDay = 0;        // Dernier jour RTC vérifié (0 = jamais)

  // État des fades
  bool fadeInActive = false;
  bool fadeOutActive = false;
  unsigned long fadeStartTime = 0;

  // Cache d'intervalle de vérification
  unsigned long lastCachedCheckInterval = 0;
  uint8_t lastCachedIntervalDay = 0;
};

#endif // SCHEDULE_STATE_H
