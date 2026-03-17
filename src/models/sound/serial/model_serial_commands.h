#ifndef MODEL_SOUND_SERIAL_COMMANDS_H
#define MODEL_SOUND_SERIAL_COMMANDS_H

#include <Arduino.h>
#include "common/managers/audio/audio_manager.h"

/**
 * Commandes Serial pour Sound
 */

class ModelSoundSerialCommands {
public:
  /**
   * Traiter une commande spécifique au modèle Sound
   * @param command La commande à traiter
   * @return true si la commande a été traitée, false sinon
   */
  static bool processCommand(const String& command);

  /**
   * Afficher l'aide des commandes spécifiques à Sound
   */
  static void printHelp();
};

#endif // MODEL_SOUND_SERIAL_COMMANDS_H
