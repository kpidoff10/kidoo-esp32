#include "schedule_utils.h"
#include "../../../common/utils/time_utils.h"

namespace ScheduleUtils {

uint8_t weekdayToIndex(uint8_t dayOfWeek) {
  // RTC dayOfWeek: 1=Lundi, 7=Dimanche
  // Notre index: 0=Lundi, 6=Dimanche
  if (dayOfWeek >= 1 && dayOfWeek <= 7) {
    return dayOfWeek - 1;
  }
  return 0; // Par défaut, lundi
}

const char* indexToWeekday(uint8_t index) {
  if (index < 7) {
    return WEEKDAY_NAMES[index];
  }
  return WEEKDAY_NAMES[0];
}

void updateCheckingState(ScheduleState& state, bool scheduleActivated) {
  bool wasEnabled = state.checkingEnabled;
  state.checkingEnabled = scheduleActivated;

  if (state.checkingEnabled) {
    if (!wasEnabled) {
      state.lastCheckTime = millis();
    }
  }
}

unsigned long calculateNextCheckInterval(
  ScheduleState& state,
  bool scheduleActivated,
  int triggerHour,
  int triggerMinute,
  uint8_t currentHour,
  uint8_t currentMinute,
  uint8_t currentDayOfWeek,
  bool configHasChanged
) {
  // Si non activé, vérifier rarement
  if (!scheduleActivated) {
    state.lastCachedCheckInterval = CHECK_INTERVAL_3H_MS;
    return state.lastCachedCheckInterval;
  }

  // Retourner l'intervalle en cache si le jour n'a pas changé et config stable
  if (currentDayOfWeek == state.lastCachedIntervalDay && !configHasChanged) {
    return state.lastCachedCheckInterval;
  }

  // Jour changé ou config changée : recalculer
  state.lastCachedIntervalDay = currentDayOfWeek;

  // Calculer les minutes jusqu'à l'heure de déclenchement
  int currentMinutes = TimeUtils::timeToMinutes(currentHour, currentMinute);
  int triggerMinutes = TimeUtils::timeToMinutes(triggerHour, triggerMinute);
  int minutesUntilTarget = triggerMinutes - currentMinutes;

  // Si l'heure de déclenchement est passée aujourd'hui, c'est pour demain
  if (minutesUntilTarget < 0) {
    minutesUntilTarget += DreamTiming::MINUTES_PER_DAY;
  }

  // Convertir en heures
  float hoursUntilTarget = minutesUntilTarget / 60.0f;

  // Déterminer l'intervalle de vérification basé sur la distance
  if (hoursUntilTarget > 6.0f) {
    state.lastCachedCheckInterval = CHECK_INTERVAL_3H_MS;
  } else if (hoursUntilTarget > 3.0f) {
    state.lastCachedCheckInterval = CHECK_INTERVAL_1H_MS;
  } else if (hoursUntilTarget > 1.0f) {
    state.lastCachedCheckInterval = CHECK_INTERVAL_30M_MS;
  } else {
    state.lastCachedCheckInterval = CHECK_INTERVAL_MS;
  }

  return state.lastCachedCheckInterval;
}

void resetTriggeredFlags(ScheduleState& state) {
  state.lastTriggeredHour = DreamTiming::TRIGGERED_NEVER;
  state.lastTriggeredMinute = DreamTiming::TRIGGERED_NEVER;
}

} // namespace ScheduleUtils
