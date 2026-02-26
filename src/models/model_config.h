#ifndef MODEL_CONFIG_H
#define MODEL_CONFIG_H

/**
 * Fichier central pour inclure la configuration du modèle
 * Généré par: node scripts/generate.js
 * Source: models.yaml
 */

#if !defined(KIDOO_MODEL_DREAM) && !defined(KIDOO_MODEL_GOTCHI)
  #error "Aucun modele Kidoo defini. Definir KIDOO_MODEL_* dans platformio.ini"
#endif

#include "common/config/default_config.h"

#ifdef KIDOO_MODEL_DREAM
  #include "dream/config/config.h"
  #include "dream/config/default_config.h"
  #define KIDOO_MODEL_NAME "Dream"
#elif defined(KIDOO_MODEL_GOTCHI)
  #include "gotchi/config/config.h"
  #include "gotchi/config/default_config.h"
  #define KIDOO_MODEL_NAME "Gotchi"
#endif

#endif // MODEL_CONFIG_H
