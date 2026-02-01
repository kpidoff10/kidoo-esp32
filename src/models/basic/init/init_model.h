#ifndef INIT_MODEL_BASIC_H
#define INIT_MODEL_BASIC_H

#include <Arduino.h>

/**
 * Initialisation spécifique au modèle Kidoo Basic
 * 
 * Ce fichier contient les fonctions d'initialisation spécifiques
 * au modèle Basic qui ne sont pas communes aux autres modèles.
 */

class InitModelBasic {
public:
  /**
   * Initialisation spécifique au modèle Basic
   * Appelée après l'initialisation des composants communs
   * 
   * @return true si l'initialisation est réussie, false sinon
   */
  static bool init();
  
  /**
   * Configuration spécifique au modèle Basic
   * Appelée avant l'initialisation des composants communs
   * 
   * @return true si la configuration est réussie, false sinon
   */
  static bool configure();
};

#endif // INIT_MODEL_BASIC_H
