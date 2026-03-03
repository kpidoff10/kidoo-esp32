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

  // IMPORTANT: Initialisé complètement en lazy (lazy initialization)
  // BLE ne sera initialisé que lorsqu'il est vraiment nécessaire :
  // - Appui long sur le bouton BLE
  // - Sortie d'usine (pas de config.json)
  // - WiFi non connecté après l'attente (auto-activation)
  //
  // Cela économise ~60KB de RAM au démarrage, tout en gardant BLE disponible pour :
  // - Configuration initiale (appareillage)
  // - Changement de WiFi (si WiFi se déconnecte)

  // Stocker le nom du device pour la réinitialisation lazy (sans initialiser rien)
  // Cela permettra à BLEConfigManager::enableBLE() d'initialiser BLE à la demande
  BLEManager::setDeviceNameForReinit(DEFAULT_DEVICE_NAME);

  // IMPORTANT: NE PAS initialiser BLEConfigManager ici pour économiser la RAM
  // BLEConfigManager sera initialisé en lazy lors de la première activation BLE
  // La détection du bouton BLE sera actif une fois BLE activé

  Serial.println("[INIT] BLE completement desactive au boot (lazy mode)");
  Serial.println("[INIT] BLE sera active a la demande (bouton, setup, WiFi change)");

  systemStatus.ble = INIT_SUCCESS;

  return true;
#endif
}