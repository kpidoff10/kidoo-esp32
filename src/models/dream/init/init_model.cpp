#include "init_model.h"
#include "common/managers/init/init_manager.h"
#include "models/dream/managers/bedtime/bedtime_manager.h"
#include "models/dream/managers/wakeup/wakeup_manager.h"

/**
 * Initialisation spécifique au modèle Kidoo Dream
 */

bool InitModelDream::configure() {
  // Configuration spécifique au Dream avant l'initialisation
  return true;
}

bool InitModelDream::init() {
  // Initialisation spécifique au Dream après l'initialisation des composants communs
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
