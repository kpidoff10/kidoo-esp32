#ifndef MODEL_SERIAL_COMMANDS_H
#define MODEL_SERIAL_COMMANDS_H

/**
 * Fichier central pour inclure les commandes Serial du modèle
 * 
 * Ce fichier inclut automatiquement les bonnes commandes Serial
 * selon le modèle défini lors de la compilation
 */

// Vérifier qu'un modèle est défini
#if !defined(KIDOO_MODEL_BASIC) && !defined(KIDOO_MODEL_MINI) && !defined(KIDOO_MODEL_DREAM) && !defined(KIDOO_MODEL_GOTCHI)
  #error "Aucun modele Kidoo defini. Definir KIDOO_MODEL_BASIC, KIDOO_MODEL_MINI, KIDOO_MODEL_DREAM ou KIDOO_MODEL_GOTCHI dans platformio.ini"
#endif

// Inclure les commandes Serial du modèle approprié
#ifdef KIDOO_MODEL_BASIC
  #include "basic/serial/model_serial_commands.h"
  #define ModelSerialCommands ModelBasicSerialCommands
#elif defined(KIDOO_MODEL_MINI)
  #include "mini/serial/model_serial_commands.h"
  #define ModelSerialCommands ModelMiniSerialCommands
#elif defined(KIDOO_MODEL_DREAM)
  #include "dream/serial/model_serial_commands.h"
  #define ModelSerialCommands ModelDreamSerialCommands
#elif defined(KIDOO_MODEL_GOTCHI)
  #include "gotchi/serial/model_serial_commands.h"
  #define ModelSerialCommands ModelGotchiSerialCommands
#endif

#endif // MODEL_SERIAL_COMMANDS_H
