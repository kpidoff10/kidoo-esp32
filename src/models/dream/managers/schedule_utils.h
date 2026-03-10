#ifndef SCHEDULE_UTILS_H
#define SCHEDULE_UTILS_H

#include <Arduino.h>
#include "dream_schedules.h"
#include "schedule_state.h"

/**
 * Utilitaires partagés pour BedtimeManager et WakeupManager.
 *
 * Regroupe les fonctions algorithmiques identiques ou quasi-identiques
 * pour éviter la duplication de code entre les deux managers.
 */
namespace ScheduleUtils {

  /**
   * Convertir un jour RTC (1-7) en index tableau (0-6)
   * RTC: 1=Lundi, 7=Dimanche
   * Index: 0=Lundi, 6=Dimanche
   */
  uint8_t weekdayToIndex(uint8_t dayOfWeek);

  /**
   * Convertir un index tableau (0-6) en string du jour
   * Utilise WEEKDAY_NAMES[] depuis dream_schedules.h
   */
  const char* indexToWeekday(uint8_t index);

  /**
   * Met à jour checkingEnabled selon l'activation du schedule pour le jour courant.
   *
   * @param state État du manager (modifié en place, pour mettre à jour checkingEnabled et lastCheckTime)
   * @param scheduleActivated Valeur du champ activated pour le jour courant
   */
  void updateCheckingState(ScheduleState& state, bool scheduleActivated);

  /**
   * Calcule l'intervalle adaptatif de vérification basé sur la distance jusqu'au déclenchement.
   *
   * Gère un cache pour éviter les recalculs si le jour et la config n'ont pas changé.
   *
   * @param state État du manager (contient cache et comparaison config)
   * @param scheduleActivated Vrai si la routine est activée pour le jour
   * @param triggerHour Heure de déclenchement (pour bedtime = schedule hour, pour wakeup = schedule hour - 15min)
   * @param triggerMinute Minute de déclenchement (pour bedtime = schedule minute, pour wakeup = schedule minute - 15min)
   * @param currentHour Heure actuelle RTC
   * @param currentMinute Minute actuelle RTC
   * @param currentDayOfWeek Jour actuel RTC (1-7)
   * @param configChanged Booléen : config a-t-elle changé depuis le dernier check
   *
   * @return Intervalle de vérification en millisecondes (CHECK_INTERVAL_*_MS)
   */
  unsigned long calculateNextCheckInterval(
    ScheduleState& state,
    bool scheduleActivated,
    int triggerHour,
    int triggerMinute,
    uint8_t currentHour,
    uint8_t currentMinute,
    uint8_t currentDayOfWeek,
    bool configChanged
  );

} // namespace ScheduleUtils

#endif // SCHEDULE_UTILS_H
