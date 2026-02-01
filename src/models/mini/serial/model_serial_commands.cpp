#include "model_serial_commands.h"
#include <Arduino.h>

/**
 * Commandes Serial spécifiques au modèle Kidoo Mini
 */

bool ModelMiniSerialCommands::processCommand(const String& command) {
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
  
  // Traiter les commandes spécifiques au Mini
  if (cmd == "mini-info") {
    Serial.println("[MINI] Informations specifiques au modele Mini");
    Serial.println("[MINI] Nombre de LEDs: 60");
    Serial.println("[MINI] Modele: Kidoo Mini");
    return true;
  }
  
  // Ajouter d'autres commandes spécifiques au Mini ici
  // Exemple:
  // if (cmd == "mini-test") {
  //   Serial.println("[MINI] Test specifique au Mini");
  //   return true;
  // }
  
  return false; // Commande non reconnue
}

void ModelMiniSerialCommands::printHelp() {
  Serial.println("");
  Serial.println("========================================");
  Serial.println("  COMMANDES SPECIFIQUES MINI");
  Serial.println("========================================");
  Serial.println("  mini-info       - Afficher les infos du modele Mini");
  Serial.println("========================================");
  Serial.println("");
}
