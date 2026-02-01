#include "model_serial_commands.h"
#include "../../common/managers/nfc/nfc_manager.h"
#include "../../model_config.h"
#include <Arduino.h>

/**
 * Commandes Serial spécifiques au modèle Kidoo Basic
 */

bool ModelBasicSerialCommands::processCommand(const String& command) {
  // Séparer la commande et les arguments
  int spaceIndex = command.indexOf(' ');
  String cmd = command;
  String args = "";
  
  if (spaceIndex > 0) {
    cmd = command.substring(0, spaceIndex);
    args = command.substring(spaceIndex + 1);
  }
  
  cmd.toLowerCase();
  cmd.trim();
  args.trim();
  
  // Traiter les commandes spécifiques au Basic
  if (cmd == "basic-info") {
    Serial.println("[BASIC] Informations specifiques au modele Basic");
    Serial.println("[BASIC] Nombre de LEDs: 144");
    Serial.println("[BASIC] Modele: Kidoo Basic");
    return true;
  }
  
  if (cmd == "nfc-test" || cmd == "nfc") {
    // Tester le module NFC
    Serial.println("");
    Serial.println("========================================");
    Serial.println("          TEST NFC");
    Serial.println("========================================");
    
    if (!NFCManager::isInitialized()) {
      Serial.println("[NFC] Non initialise");
      Serial.println("[NFC] Tentative d'initialisation...");
      
      if (!NFCManager::init()) {
        Serial.println("[NFC] ERREUR: Echec de l'initialisation");
        Serial.println("========================================");
        return true;
      }
    }
    
    bool available = NFCManager::isAvailable();
    Serial.print("[NFC] Statut: ");
    if (available) {
      Serial.println("Operationnel");
      
      uint32_t version = NFCManager::getFirmwareVersion();
      if (version > 0) {
        Serial.print("[NFC] Version firmware: 0x");
        Serial.println(version, HEX);
      } else {
        Serial.println("[NFC] Version firmware: Non disponible");
      }
    } else {
      Serial.println("Non operationnel");
      Serial.println("[NFC] WARNING: Module NFC non detecte ou non configure");
      Serial.println("[NFC] Verifiez les connexions et la configuration des pins");
    }
    
    Serial.println("========================================");
    return true;
  }
  
  return false; // Commande non reconnue
}

void ModelBasicSerialCommands::printHelp() {
  Serial.println("");
  Serial.println("========================================");
  Serial.println("  COMMANDES SPECIFIQUES BASIC");
  Serial.println("========================================");
  Serial.println("  basic-info      - Afficher les infos du modele Basic");
  #ifdef HAS_NFC
  if (HAS_NFC) {
    Serial.println("  nfc-test, nfc   - Tester la detection du module NFC");
  }
  #endif
  Serial.println("========================================");
  Serial.println("");
}
