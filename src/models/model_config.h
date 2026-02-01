#ifndef MODEL_CONFIG_H
#define MODEL_CONFIG_H

/**
 * Fichier central pour inclure la configuration du modèle
 * 
 * Ce fichier inclut automatiquement la bonne configuration
 * selon le modèle défini lors de la compilation
 */

// Vérifier qu'un modèle est défini
#if !defined(KIDOO_MODEL_BASIC) && !defined(KIDOO_MODEL_MINI) && !defined(KIDOO_MODEL_DREAM)
  #error "Aucun modele Kidoo defini. Definir KIDOO_MODEL_BASIC, KIDOO_MODEL_MINI ou KIDOO_MODEL_DREAM dans platformio.ini"
#endif

// Inclure la configuration commune
#include "common/config/default_config.h"

// Inclure la configuration du modèle approprié
#ifdef KIDOO_MODEL_BASIC
  #include "basic/config/config.h"
  #include "basic/config/default_config.h"
  #define KIDOO_MODEL_NAME "Basic"
#elif defined(KIDOO_MODEL_MINI)
  #include "mini/config/config.h"
  #include "mini/config/default_config.h"
  #define KIDOO_MODEL_NAME "Mini"
#elif defined(KIDOO_MODEL_DREAM)
  #include "dream/config/config.h"
  #include "dream/config/default_config.h"
  #define KIDOO_MODEL_NAME "Dream"
#endif

#endif // MODEL_CONFIG_H
