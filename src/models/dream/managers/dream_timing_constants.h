#ifndef DREAM_TIMING_CONSTANTS_H
#define DREAM_TIMING_CONSTANTS_H

/**
 * Constantes de timing centralisées pour BedtimeManager et WakeupManager.
 *
 * Évite les magic numbers (24 * 60, etc.) dispersés dans le code.
 */
namespace DreamTiming {

/** Minutes dans une journée (24h * 60min) */
constexpr int MINUTES_PER_DAY = 24 * 60;

/** Valeur sentinelle pour "jamais déclenché" (lastTriggeredHour/Minute) */
constexpr uint8_t TRIGGERED_NEVER = 255;

/** Durées de transition (fade-in/fade-out) en millisecondes */
constexpr unsigned long FADE_IN_DURATION_MS = 60000;      // 1 minute (transition vers couleur réveil)
constexpr unsigned long FADE_OUT_DURATION_MS = 300000;    // 5 minutes (extinction progressive)
constexpr unsigned long FADE_UPDATE_INTERVAL_MS = 100;    // 100ms (throttling des mises à jour LED)

}  // namespace DreamTiming

#endif  // DREAM_TIMING_CONSTANTS_H
