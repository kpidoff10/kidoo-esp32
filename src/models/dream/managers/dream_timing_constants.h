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

}  // namespace DreamTiming

#endif  // DREAM_TIMING_CONSTANTS_H
