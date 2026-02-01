#include "init_model.h"
#include "../../common/managers/init/init_manager.h"
#include "../nfc/nfc_tag_handler.h"

/**
 * Initialisation spécifique au modèle Kidoo Basic
 */

bool InitModelBasic::configure() {
  // Configuration spécifique au Basic avant l'initialisation
  Serial.println("[INIT] Configuration modele Basic");
  
  return true;
}

bool InitModelBasic::init() {
  // Initialisation spécifique au Basic après l'initialisation des composants communs
  Serial.println("");
  Serial.println("========================================");
  Serial.println("[INIT-BASIC] Initialisation modele Basic");
  Serial.println("========================================");
  
  // Initialiser le gestionnaire de tags NFC
  // Cela configure les actions automatiques quand un tag est détecté
  Serial.println("[INIT-BASIC] Init NFCTagHandler...");
  NFCTagHandler::init();
  Serial.println("[INIT-BASIC] NFCTagHandler OK");
  
  return true;
}
