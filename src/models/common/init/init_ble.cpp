#include "../managers/init/init_manager.h"
#include "../managers/ble/ble_manager.h"
#include "../managers/ble_config/ble_config_manager.h"
#include "../../model_config.h"

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
  
  // Utiliser DEFAULT_DEVICE_NAME pour le nom BLE
  // DEFAULT_DEVICE_NAME est défini dans le fichier default_config.h du modèle
  const char* deviceName = DEFAULT_DEVICE_NAME;
  
  // Initialiser le BLEManager (mais ne pas démarrer l'advertising)
  if (!BLEManager::init(deviceName)) {
    systemStatus.ble = INIT_FAILED;
    Serial.println("[INIT] ERREUR: Echec initialisation BLE");
    return false;
  }
  
  if (!BLEManager::isAvailable()) {
    systemStatus.ble = INIT_FAILED;
    Serial.println("[INIT] WARNING: BLE non disponible");
    return false;
  }
  
  // Initialiser le BLEConfigManager avec le pin du bouton depuis la config
  #ifdef BLE_CONFIG_BUTTON_PIN
  if (!BLEConfigManager::init(BLE_CONFIG_BUTTON_PIN)) {
    Serial.println("[INIT] WARNING: Echec initialisation BLEConfigManager");
    // Ne pas faire échouer l'init BLE si le bouton n'est pas disponible
  }
  #else
  Serial.println("[INIT] WARNING: BLE_CONFIG_BUTTON_PIN non defini dans config.h");
  #endif
  
  // IMPORTANT: Ne PAS démarrer l'advertising automatiquement
  // Le BLE sera activé uniquement via appui long sur le bouton
  Serial.println("[INIT] BLE initialise (advertising desactive par defaut)");
  Serial.println("[INIT] Appui long sur bouton pour activer le BLE");
  
  systemStatus.ble = INIT_SUCCESS;
  
  return true;
#endif
}