#include "../../../model_config.h"
#ifdef HAS_VIBRATOR
#include "vibrator_manager.h"

#ifndef VIBRATOR_PIN
#define VIBRATOR_PIN 25
#endif

// PWM — fréquence basse (500 Hz) pour un couple perçu plus fort sur les vibreurs type moteur DC
static const int VIBRATOR_PWM_CHANNEL = 0;
static const int VIBRATOR_PWM_FREQ     = 500;
static const int VIBRATOR_PWM_RESOLUTION = 8;  // 0-255

bool VibratorManager::initialized = false;
uint8_t VibratorManager::currentIntensity = 255;
bool VibratorManager::currentOn = false;

bool VibratorManager::init() {
  if (initialized) {
    return true;
  }

  Serial.printf("[VIBRATOR] Init pin GPIO %d (PWM)\n", VIBRATOR_PIN);

  pinMode(VIBRATOR_PIN, OUTPUT);
  ledcSetup(VIBRATOR_PWM_CHANNEL, VIBRATOR_PWM_FREQ, VIBRATOR_PWM_RESOLUTION);
  ledcAttachPin(VIBRATOR_PIN, VIBRATOR_PWM_CHANNEL);

  ledcWrite(VIBRATOR_PWM_CHANNEL, 0);
  currentOn = false;
  currentIntensity = 255;
  initialized = true;

  Serial.println("[VIBRATOR] OK");
  return true;
}

bool VibratorManager::isInitialized() {
  return initialized;
}

void VibratorManager::setIntensity(uint8_t value) {
  if (!initialized) return;
  currentIntensity = value;
  if (currentOn) {
    ledcWrite(VIBRATOR_PWM_CHANNEL, currentIntensity);
  }
}

uint8_t VibratorManager::getIntensity() {
  return currentIntensity;
}

void VibratorManager::setOn(bool on) {
  if (!initialized) return;
  currentOn = on;
  ledcWrite(VIBRATOR_PWM_CHANNEL, on ? currentIntensity : 0);
}

bool VibratorManager::isOn() {
  return initialized && currentOn;
}

void VibratorManager::pulse(uint32_t durationMs, uint8_t intensity) {
  if (!initialized) return;
  setIntensity(intensity);
  setOn(true);
  // L'arrêt après durationMs doit être géré par l'appelant (delay ou tâche).
  // Pour un usage simple depuis Serial, l'appelant fera delay(durationMs) puis stop().
}

void VibratorManager::stop() {
  if (!initialized) return;
  currentOn = false;
  ledcWrite(VIBRATOR_PWM_CHANNEL, 0);
}

bool VibratorManager::playEffect(Effect effect) {
  if (!initialized) return false;

  // Forcer l'intensité au max pendant les effets pour un rendu plus fort
  setIntensity(255);

  switch (effect) {
    case EFFECT_SHORT: {
      // Court : ~120 ms
      setOn(true);
      delay(120);
      stop();
      break;
    }
    case EFFECT_LONG: {
      // Long : ~800 ms
      setOn(true);
      delay(800);
      stop();
      break;
    }
    case EFFECT_JERKY: {
      // Saccadé : 5 micro-impulsions 50 ms on / 50 ms off
      for (int i = 0; i < 5; i++) {
        setOn(true);
        delay(50);
        stop();
        delay(50);
      }
      break;
    }
    case EFFECT_PULSE: {
      // Pulsation : 3 cycles 200 ms on / 200 ms off
      for (int i = 0; i < 3; i++) {
        setOn(true);
        delay(200);
        stop();
        delay(200);
      }
      break;
    }
    case EFFECT_DOUBLE_TAP: {
      // Double tap : toc - pause - toc
      setOn(true);
      delay(80);
      stop();
      delay(120);
      setOn(true);
      delay(80);
      stop();
      break;
    }
  }
  return true;
}

void VibratorManager::printStatus() {
  if (!Serial) return;
  Serial.println("");
  Serial.println("========== Vibreur ==========");
  if (!initialized) {
    Serial.println("[VIBRATOR] Non initialise");
    Serial.println("============================");
    return;
  }
  Serial.printf("[VIBRATOR] Pin: GPIO %d\n", VIBRATOR_PIN);
  Serial.printf("[VIBRATOR] Etat: %s\n", currentOn ? "ON" : "OFF");
  Serial.printf("[VIBRATOR] Intensite: %d/255\n", currentIntensity);
  Serial.println("============================");
}

#endif // HAS_VIBRATOR
