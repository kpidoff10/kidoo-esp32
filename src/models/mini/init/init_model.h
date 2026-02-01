#ifndef INIT_MODEL_MINI_H
#define INIT_MODEL_MINI_H

#include <Arduino.h>

/**
 * Initialisation spécifique au modèle Kidoo Mini
 * 
 * Ce fichier contient les fonctions d'initialisation spécifiques
 * au modèle Mini qui ne sont pas communes aux autres modèles.
 */

class InitModelMini {
public:
  /**
   * Initialisation spécifique au modèle Mini
   * Appelée après l'initialisation des composants communs
   * 
   * @return true si l'initialisation est réussie, false sinon
   */
  static bool init();
  
  /**
   * Configuration spécifique au modèle Mini
   * Appelée avant l'initialisation des composants communs
   * 
   * @return true si la configuration est réussie, false sinon
   */
  static bool configure();
};

#endif // INIT_MODEL_MINI_H
