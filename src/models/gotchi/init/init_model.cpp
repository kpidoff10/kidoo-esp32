#include "init_model.h"
#include "../../model_config.h"
#include "../../common/managers/init/init_manager.h"
#ifdef HAS_LCD
#include "../../common/managers/lcd/lcd_manager.h"
#endif
#ifdef HAS_LED
#include "../../common/managers/led/led_manager.h"
#endif
#ifdef HAS_LCD
#include "../managers/emotions/emotion_manager.h"
#endif

/**
 * Initialisation spécifique au modèle Kidoo Gotchi
 */

bool InitModelGotchi::configure() {
  Serial.println("[INIT] Configuration modele Gotchi");
  return true;
}

bool InitModelGotchi::init() {
  Serial.println("");
  Serial.println("========================================");
  Serial.println("[INIT-GOTCHI] Initialisation modele Gotchi");
  Serial.println("========================================");

#ifdef HAS_LCD
  if (LCDManager::isAvailable()) {
    LCDManager::fillScreen(LCDManager::COLOR_BLACK);
    LCDManager::setTextColor(LCDManager::COLOR_GREEN);
    LCDManager::setTextSize(3);
    LCDManager::setCursor(40, 100);
    LCDManager::println("Kidoo");
    LCDManager::setCursor(50, 140);
    LCDManager::println("Gotchi");
    LCDManager::setTextSize(1);
    LCDManager::setTextColor(LCDManager::COLOR_WHITE);
    LCDManager::setCursor(30, 200);
    LCDManager::println("Demarrage...");
    delay(1500);
    EmotionManager::init();
    EmotionManager::setEmotion(Emotion::Happy);  // Visage Tamagotchi qui sourit
  }
#endif

  // Gotchi : toujours allumer la LED au boot (même en mode BLE / sans WiFi)
  // pour confirmer que la carte et la LED intégrée (GPIO 48) fonctionnent
#ifdef HAS_LED
  if (HAS_LED && LEDManager::isInitialized()) {
    LEDManager::setColor(0, 255, 0);  // vert (évite conflit avec macro COLOR_* de colors.h)
    LEDManager::setEffect(LED_EFFECT_NONE);
  }
#endif

  return true;
}
