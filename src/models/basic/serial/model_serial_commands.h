#ifndef MODEL_BASIC_SERIAL_COMMANDS_H
#define MODEL_BASIC_SERIAL_COMMANDS_H

#include <Arduino.h>

/**
 * Commandes Serial spécifiques au modèle Kidoo Basic
 * 
 * Ce fichier contient les commandes Serial spécifiques
 * au modèle Basic qui ne sont pas communes aux autres modèles
 */

class ModelBasicSerialCommands {
public:
  /**
   * Traiter une commande spécifique au modèle Basic
   * @param command La commande à traiter
   * @return true si la commande a été traitée, false sinon
   */
  static bool processCommand(const String& command);
  
  /**
   * Afficher l'aide des commandes spécifiques au Basic
   */
  static void printHelp();
};

#endif // MODEL_BASIC_SERIAL_COMMANDS_H
