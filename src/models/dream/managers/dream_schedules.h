#ifndef DREAM_SCHEDULES_H
#define DREAM_SCHEDULES_H

#include <cstddef>

/**
 * Constantes partagées pour le planning bedtime/wakeup
 */

// Noms des jours de la semaine (pour parsing JSON et affichage)
static const char* WEEKDAY_NAMES[] = {
  "monday",
  "tuesday",
  "wednesday",
  "thursday",
  "friday",
  "saturday",
  "sunday"
};

// Intervalles de vérification adaptatifs basés sur la distance à l'heure déclenchement
static const unsigned long CHECK_INTERVAL_MS = 60000;        // 1 minute (quand proche de l'heure)
static const unsigned long CHECK_INTERVAL_30M_MS = 1800000;  // 30 minutes (1-3 heures avant)
static const unsigned long CHECK_INTERVAL_1H_MS = 3600000;   // 1 heure (3-6 heures avant)
static const unsigned long CHECK_INTERVAL_3H_MS = 10800000;  // 3 heures (>6 heures avant)

#endif // DREAM_SCHEDULES_H
