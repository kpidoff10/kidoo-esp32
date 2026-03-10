#include "common/managers/init/init_manager.h"
#include "common/managers/led/led_manager.h"
#include "common/managers/log/log_manager.h"
#include "color/colors.h"

#ifdef HAS_SD
#include "common/managers/device_key/device_key_manager.h"
#endif

bool InitManager::initDeviceKey() {
  systemStatus.deviceKey = INIT_IN_PROGRESS;

#ifndef HAS_SD
  // Sans SD, impossible de gérer les clés
  systemStatus.deviceKey = INIT_NOT_STARTED;
  return true;  // Pas une erreur, juste désactivé
#else
  if (!HAS_SD) {
    systemStatus.deviceKey = INIT_NOT_STARTED;
    return true;  // Pas une erreur, juste désactivé
  }

  // Vérifier si la clé device existe (sans créer)
  if (!DeviceKeyManager::keyExists()) {
    // Clé device manquante -> mode "no config"
    systemStatus.deviceKey = INIT_FAILED;

    if (Serial) {
      Serial.println("[INIT] Clé device manquante - configuration requise");
    }

    // Définir le flag pour forcer l'activation BLE auto
    InitManager::noDeviceKeyFound = true;

    // Passer en mode "no config" : respiration rouge rapide
    #ifdef HAS_LED
    if (HAS_LED && systemStatus.led == INIT_SUCCESS) {
      LEDManager::setColor(COLOR_ERROR);
      LEDManager::setEffect(LED_EFFECT_PULSE);
      if (Serial) {
        Serial.println("[INIT] Mode NO CONFIG: respiration rouge activée");
      }
    }
    #endif

    return false;
  }

  systemStatus.deviceKey = INIT_SUCCESS;

  if (Serial) {
    Serial.println("[INIT] Clé device OK");
  }

  return true;
#endif
}
