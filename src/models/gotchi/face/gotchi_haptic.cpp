#include "gotchi_haptic.h"
#include "models/model_config.h"

#ifdef HAS_VIBRATOR
#include "common/managers/vibrator/vibrator_manager.h"
#include <Arduino.h>

namespace {
// Rate limit global : empêche deux pulses de s'écraser l'une l'autre avant
// que le moteur DC n'ait eu le temps de démarrer (~50ms minimum).
// Sans cette garde, des évènements rapprochés (rebonds en cascade, ballCatch
// + ballBounce dans la meme frame) feraient avorter la vibration en cours.
constexpr uint32_t MIN_GAP_MS = 80;
uint32_t s_lastPulseAt = 0;

inline bool canPulse() {
  uint32_t now = millis();
  if (now - s_lastPulseAt < MIN_GAP_MS) return false;
  s_lastPulseAt = now;
  return true;
}

inline void fire(uint32_t durMs, uint8_t intensity) {
  if (!canPulse()) return;
  VibratorManager::pulseAuto(durMs, intensity);
}
} // namespace

namespace GotchiHaptic {

void update() { VibratorManager::update(); }

// Note durées : un moteur DC vibrant met ~30-50ms à démarrer.
// On ne descend jamais sous 60ms sinon le rotor n'a pas le temps de tourner.
// Et on garde l'intensité haute (>=200) pour un démarrage franc.

// --- Jeu ---
void ballBounce() { fire(70, 255); }
void ballThrow()  { fire(120, 255); }
void ballCatch()  { fire(60, 220); }

// --- Émotions ---
void joyTap()     { fire(100, 240); }
void angerBurst() { fire(220, 255); }
void angerShake() { fire(70, 240); }
void chew()       { fire(80, 220); }
void cough()      { fire(150, 200); }
void heartBeat()  { fire(80, 220); }

} // namespace GotchiHaptic

#else // !HAS_VIBRATOR

namespace GotchiHaptic {
void update() {}
void ballBounce() {}
void ballThrow() {}
void ballCatch() {}
void joyTap() {}
void angerBurst() {}
void angerShake() {}
void chew() {}
void cough() {}
void heartBeat() {}
} // namespace GotchiHaptic

#endif // HAS_VIBRATOR
