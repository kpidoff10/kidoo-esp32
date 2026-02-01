#ifndef MODEL_DREAM_SERIAL_COMMANDS_H
#define MODEL_DREAM_SERIAL_COMMANDS_H

#include <Arduino.h>

/**
 * Commandes Serial spécifiques au modèle Kidoo Dream
 * 
 * Ce fichier contient les commandes Serial spécifiques
 * au modèle Dream qui ne sont pas communes aux autres modèles
 */

class ModelDreamSerialCommands {
public:
  /**
   * Traiter une commande spécifique au modèle Dream
   * @param command La commande à traiter
   * @return true si la commande a été traitée, false sinon
   */
  static bool processCommand(const String& command);
  
  /**
   * Afficher l'aide des commandes spécifiques au Dream
   */
  static void printHelp();
};

#endif // MODEL_DREAM_SERIAL_COMMANDS_H
