#include "../managers/init/init_manager.h"
#include "../managers/nfc/nfc_manager.h"
#include "../../model_config.h"

bool InitManager::initNFC() {
  systemStatus.nfc = INIT_IN_PROGRESS;
  
#ifndef HAS_NFC
  systemStatus.nfc = INIT_NOT_STARTED;
  return true;  // Pas une erreur, juste désactivé
#else
  if (!NFCManager::init()) {
    systemStatus.nfc = INIT_FAILED;
    
    // Afficher un warning si le NFC n'est pas opérationnel
    Serial.println("[INIT] WARNING: NFC non operationnel (module non detecte ou non configure)");
    
    return false;
  }
  
  if (!NFCManager::isAvailable()) {
    systemStatus.nfc = INIT_FAILED;
    
    // Afficher un warning si le NFC n'est pas disponible
    Serial.println("[INIT] WARNING: NFC non disponible (hardware non operationnel)");
    
    return false;
  }
  
  systemStatus.nfc = INIT_SUCCESS;
  Serial.println("[INIT] NFC operationnel");
  
  return true;
#endif
}
