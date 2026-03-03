#include "common/managers/init/init_manager.h"
#include "common/managers/ble/ble_manager.h"
#include "common/managers/ble_config/ble_config_manager.h"
#include "models/model_config.h"

bool InitManager::initBLE() {
  systemStatus.ble = INIT_IN_PROGRESS;

#ifndef HAS_BLE
  systemStatus.ble = INIT_NOT_STARTED;
  return true;  // Pas une erreur, juste désactivé
#else
  if (!HAS_BLE) {
    systemStatus.ble = INIT_NOT_STARTED;
    return true;  // Pas une erreur, juste désactivé
  }

  // IMPORTANT: Initialisé en lazy (lazy initialization)
  // BLEManager (structures BLE, tasks) ne sera créé que lorsqu'il est vraiment nécessaire
  // BLEConfigManager (bouton) s'initialise au boot pour détecter les appuis
  //
  // Cela économise ~60KB de RAM au démarrage, tout en gardant le bouton détectable dès le boot
  //
  // Activation BLE seulement sur :
  // - Appui long sur le bouton BLE (3 secondes)

  // Stocker le nom du device pour la réinitialisation lazy
  BLEManager::setDeviceNameForReinit(DEFAULT_DEVICE_NAME);

  // Initialiser UNIQUEMENT BLEConfigManager (très petit - détection bouton)
  // Ne PAS initialiser BLEManager ici - sera fait en lazy au premier appui bouton
  #ifdef BLE_CONFIG_BUTTON_PIN
  if (!BLEConfigManager::init(BLE_CONFIG_BUTTON_PIN)) {
    Serial.println("[INIT] WARNING: Echec initialisation BLEConfigManager");
    // Ne pas faire échouer l'init si le bouton n'est pas disponible
  }
  #else
  Serial.println("[INIT] WARNING: BLE_CONFIG_BUTTON_PIN non defini dans config.h");
  #endif

  Serial.println("[INIT] BLE desactive au boot (lazy mode)");
  Serial.println("[INIT] Appui long (3s) sur bouton pour activer le BLE");

  systemStatus.ble = INIT_SUCCESS;

  return true;
#endif
}