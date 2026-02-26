#ifndef MODEL_INIT_H
#define MODEL_INIT_H

/**
 * Fichier central pour inclure l'initialisation du modèle
 * Généré par: node scripts/generate.js
 * Source: models.yaml
 */

#if !defined(KIDOO_MODEL_DREAM) && !defined(KIDOO_MODEL_GOTCHI)
  #error "Aucun modele Kidoo defini. Definir KIDOO_MODEL_* dans platformio.ini"
#endif

#ifdef KIDOO_MODEL_DREAM
  #include "dream/init/init_model.h"
  #define InitModel InitModelDream
#elif defined(KIDOO_MODEL_GOTCHI)
  #include "gotchi/init/init_model.h"
  #define InitModel InitModelGotchi
#endif

#endif // MODEL_INIT_H
