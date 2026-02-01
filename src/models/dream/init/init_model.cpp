#include "init_model.h"
#include "../../common/managers/init/init_manager.h"
#include "../managers/bedtime/bedtime_manager.h"
#include "../managers/wakeup/wakeup_manager.h"

/**
 * Initialisation spécifique au modèle Kidoo Dream
 */

bool InitModelDream::configure() {
  // Configuration spécifique au Dream avant l'initialisation
  Serial.println("[INIT] Configuration modele Dream");
  
  return true;
}

bool InitModelDream::init() {
  // Initialisation spécifique au Dream après l'initialisation des composants communs
  Serial.println("");
  Serial.println("========================================");
  Serial.println("[INIT-DREAM] Initialisation modele Dream");
  Serial.println("========================================");
  
  // Note: Dream n'a pas de NFC, donc pas d'initialisation du handler NFC
  
  // Initialiser le gestionnaire bedtime automatique
  if (!BedtimeManager::init()) {
    Serial.println("[INIT-DREAM] ERREUR: Echec initialisation BedtimeManager");
    // Ne pas bloquer l'initialisation si le bedtime échoue
  }
  
  // Initialiser le gestionnaire wake-up automatique
  if (!WakeupManager::init()) {
    Serial.println("[INIT-DREAM] ERREUR: Echec initialisation WakeupManager");
    // Ne pas bloquer l'initialisation si le wake-up échoue
  }
  
  return true;
}
