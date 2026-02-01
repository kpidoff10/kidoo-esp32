#include "../managers/init/init_manager.h"
#include "../managers/potentiometer/potentiometer_manager.h"
#include "../../model_config.h"

bool InitManager::initPotentiometer() {
#ifndef HAS_POTENTIOMETER
  systemStatus.potentiometer = INIT_NOT_STARTED;
  return true;
#else
  systemStatus.potentiometer = INIT_IN_PROGRESS;
  Serial.println("[INIT] Initialisation Potentiometre...");
  
  if (!PotentiometerManager::init()) {
    Serial.println("[INIT] ERREUR: Potentiometre non disponible");
    systemStatus.potentiometer = INIT_FAILED;
    return false;
  }
  
  if (!PotentiometerManager::isAvailable()) {
    Serial.println("[INIT] WARNING: Potentiometre non detecte");
    systemStatus.potentiometer = INIT_FAILED;
    return false;
  }
  
  // Pas de callback par défaut - sera configuré plus tard selon l'usage
  // PotentiometerManager::setCallback(myCallback);
  
  // Seuil de 3% pour éviter les faux positifs
  PotentiometerManager::setThreshold(3);
  
  systemStatus.potentiometer = INIT_SUCCESS;
  Serial.println("[INIT] Potentiometre operationnel");
  
  return true;
#endif
}
