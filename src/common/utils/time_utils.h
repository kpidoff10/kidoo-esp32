#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <Arduino.h>
#include <limits.h>

/**
 * Utilitaires pour les calculs de temps avec gestion du wrap-around de millis()
 */
namespace TimeUtils {

/**
 * Calculer le temps écoulé en gérant le wrap-around de millis()
 * qui se produit après ~49 jours
 *
 * @param currentTime Temps actuel (millis())
 * @param lastTime Temps précédent
 * @return Temps écoulé en millisecondes
 */
inline unsigned long calculateElapsed(unsigned long currentTime, unsigned long lastTime) {
  if (currentTime >= lastTime) {
    return currentTime - lastTime;
  } else {
    // Wrap-around détecté
    return (ULONG_MAX - lastTime) + currentTime + 1;
  }
}

/**
 * Convertir heure et minute en nombre total de minutes depuis minuit.
 * @param hour Heure (0-23)
 * @param minute Minute (0-59)
 * @return Minutes depuis minuit (0 à 1439)
 */
inline int timeToMinutes(int hour, int minute) {
  return hour * 60 + minute;
}

}  // namespace TimeUtils

#endif  // TIME_UTILS_H
