#ifndef MODEL_SERIAL_COMMANDS_H
#define MODEL_SERIAL_COMMANDS_H

/**
 * Fichier central pour inclure les commandes Serial du modèle
 * Généré par: node scripts/generate.js
 * Source: models.yaml
 */

#if !defined(KIDOO_MODEL_DREAM) && !defined(KIDOO_MODEL_GOTCHI)
  #error "Aucun modele Kidoo defini. Definir KIDOO_MODEL_* dans platformio.ini"
#endif

#ifdef KIDOO_MODEL_DREAM
  #include "dream/serial/model_serial_commands.h"
  #define ModelSerialCommands ModelDreamSerialCommands
#elif defined(KIDOO_MODEL_GOTCHI)
  #include "gotchi/serial/model_serial_commands.h"
  #define ModelSerialCommands ModelGotchiSerialCommands
#endif

#endif // MODEL_SERIAL_COMMANDS_H
