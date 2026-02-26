#include "common/managers/init/init_manager.h"
#include "common/managers/led/led_manager.h"
#include "common/managers/sd/sd_manager.h"
#include "color/colors.h"
#include "models/model_config.h"

bool InitManager::initLED() {
  systemStatus.led = INIT_IN_PROGRESS;
  
#ifndef HAS_LED
  systemStatus.led = INIT_NOT_STARTED;
  return true;  // Pas une erreur, juste désactivé
#else
  if (!HAS_LED) {
    systemStatus.led = INIT_NOT_STARTED;
    return true;  // Pas une erreur, juste désactivé
  }
  
  if (!LEDManager::init()) {
    systemStatus.led = INIT_FAILED;
    return false;
  }
  
  systemStatus.led = INIT_SUCCESS;
  
  // Configuration initiale des LEDs : orange qui tourne (prêt)
  // SAUF si pas de WiFi configuré : dans ce cas, BLE sera activé auto, donc pas de retour lumineux
  #ifdef HAS_WIFI
  if (HAS_WIFI) {
    const SDConfig& config = InitManager::getConfig();
    if (strlen(config.wifi_ssid) > 0) {
      // WiFi configuré -> orange pour indiquer l'init (prêt)
      LEDManager::setColor(COLOR_ORANGE);
      LEDManager::setEffect(LED_EFFECT_ROTATE);
    } else {
      // Pas de WiFi configuré -> BLE auto, pas de retour lumineux, LEDs éteintes
      LEDManager::setEffect(LED_EFFECT_NONE);
      LEDManager::setColor(0, 0, 0);
      LEDManager::clear();
    }
  } else {
    // Pas de WiFi hardware -> orange pour indiquer l'init (prêt)
    LEDManager::setColor(COLOR_ORANGE);
    LEDManager::setEffect(LED_EFFECT_ROTATE);
  }
  #else
  // Pas de WiFi hardware -> orange pour indiquer l'init (prêt)
  LEDManager::setColor(COLOR_ORANGE);
  LEDManager::setEffect(LED_EFFECT_ROTATE);
  #endif
  
  return true;
#endif
}
