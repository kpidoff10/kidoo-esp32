#ifndef MODEL_GOTCHI_SERIAL_COMMANDS_H
#define MODEL_GOTCHI_SERIAL_COMMANDS_H

#include <Arduino.h>

/**
 * Commandes Serial spécifiques au modèle Kidoo Gotchi
 */

class ModelGotchiSerialCommands {
public:
  static bool processCommand(const String& command);
  static void printHelp();
};

#endif // MODEL_GOTCHI_SERIAL_COMMANDS_H
