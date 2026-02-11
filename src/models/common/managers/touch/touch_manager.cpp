#include "../../../model_config.h"
#ifdef HAS_TOUCH
#include "touch_manager.h"

#ifndef TOUCH_PIN
#define TOUCH_PIN 5
#endif

// TTP223 : sortie HIGH = touché, LOW = non touché (mode direct)
// Pull-down interne pour état stable si le module est en open-drain
#define TOUCH_ACTIVE_LEVEL HIGH

bool TouchManager::initialized = false;
bool TouchManager::debouncedState = false;
bool TouchManager::lastRawState = false;
uint32_t TouchManager::lastChangeTime = 0;
uint32_t TouchManager::debounceMs = 50;

bool TouchManager::init() {
  if (initialized) {
    return true;
  }

  Serial.printf("[TOUCH] Init pin GPIO %d (entree digitale)\n", TOUCH_PIN);

  pinMode(TOUCH_PIN, INPUT_PULLDOWN);
  debouncedState = (digitalRead(TOUCH_PIN) == TOUCH_ACTIVE_LEVEL);
  lastRawState = debouncedState;
  lastChangeTime = millis();
  debounceMs = 50;
  initialized = true;

  Serial.println("[TOUCH] OK (TTP223)");
  return true;
}

bool TouchManager::isInitialized() {
  return initialized;
}

void TouchManager::setDebounceMs(uint32_t ms) {
  debounceMs = ms;
}

bool TouchManager::readRaw() {
  if (!initialized) return false;
  return (digitalRead(TOUCH_PIN) == TOUCH_ACTIVE_LEVEL);
}

void TouchManager::update() {
  if (!initialized) return;

  bool raw = (digitalRead(TOUCH_PIN) == TOUCH_ACTIVE_LEVEL);
  uint32_t now = millis();

  if (raw != lastRawState) {
    lastRawState = raw;
    lastChangeTime = now;
  }

  if (now - lastChangeTime >= debounceMs) {
    debouncedState = lastRawState;
  }
}

bool TouchManager::isTouched() {
  return initialized && debouncedState;
}

void TouchManager::printStatus() {
  if (!Serial) return;
  Serial.println("");
  Serial.println("========== Touch (TTP223) ==========");
  if (!initialized) {
    Serial.println("[TOUCH] Non initialise");
    Serial.println("===================================");
    return;
  }
  Serial.printf("[TOUCH] Pin: GPIO %d\n", TOUCH_PIN);
  Serial.printf("[TOUCH] Etat (debounce): %s\n", debouncedState ? "TOUCHE" : "RELACHE");
  Serial.printf("[TOUCH] Brut: %s\n", readRaw() ? "HIGH" : "LOW");
  Serial.printf("[TOUCH] Debounce: %lu ms\n", (unsigned long)debounceMs);
  Serial.println("===================================");
}

#endif // HAS_TOUCH
