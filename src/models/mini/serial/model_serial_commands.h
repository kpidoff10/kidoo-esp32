#ifndef MODEL_MINI_SERIAL_COMMANDS_H
#define MODEL_MINI_SERIAL_COMMANDS_H

#include <Arduino.h>

/**
 * Commandes Serial spécifiques au modèle Kidoo Mini
 * 
 * Ce fichier contient les commandes Serial spécifiques
 * au modèle Mini qui ne sont pas communes aux autres modèles
 */

class ModelMiniSerialCommands {
public:
  /**
   * Traiter une commande spécifique au modèle Mini
   * @param command La commande à traiter
   * @return true si la commande a été traitée, false sinon
   */
  static bool processCommand(const String& command);
  
  /**
   * Afficher l'aide des commandes spécifiques au Mini
   */
  static void printHelp();
};

#endif // MODEL_MINI_SERIAL_COMMANDS_H
