#ifndef MODEL_INIT_H
#define MODEL_INIT_H

/**
 * Fichier central pour inclure l'initialisation du modèle
 * 
 * Ce fichier inclut automatiquement la bonne initialisation
 * selon le modèle défini lors de la compilation
 */

// Vérifier qu'un modèle est défini
#if !defined(KIDOO_MODEL_BASIC) && !defined(KIDOO_MODEL_MINI) && !defined(KIDOO_MODEL_DREAM)
  #error "Aucun modele Kidoo defini. Definir KIDOO_MODEL_BASIC, KIDOO_MODEL_MINI ou KIDOO_MODEL_DREAM dans platformio.ini"
#endif

// Inclure l'initialisation du modèle approprié
#ifdef KIDOO_MODEL_BASIC
  #include "basic/init/init_model.h"
  #define InitModel InitModelBasic
#elif defined(KIDOO_MODEL_MINI)
  #include "mini/init/init_model.h"
  #define InitModel InitModelMini
#elif defined(KIDOO_MODEL_DREAM)
  #include "dream/init/init_model.h"
  #define InitModel InitModelDream
#endif

#endif // MODEL_INIT_H
