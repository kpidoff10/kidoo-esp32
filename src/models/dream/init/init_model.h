#ifndef INIT_MODEL_DREAM_H
#define INIT_MODEL_DREAM_H

#include <Arduino.h>

/**
 * Initialisation spécifique au modèle Kidoo Dream
 * 
 * Ce fichier contient les fonctions d'initialisation spécifiques
 * au modèle Dream qui ne sont pas communes aux autres modèles.
 */

class InitModelDream {
public:
  /**
   * Initialisation spécifique au modèle Dream
   * Appelée après l'initialisation des composants communs
   * 
   * @return true si l'initialisation est réussie, false sinon
   */
  static bool init();
  
  /**
   * Configuration spécifique au modèle Dream
   * Appelée avant l'initialisation des composants communs
   * 
   * @return true si la configuration est réussie, false sinon
   */
  static bool configure();
};

#endif // INIT_MODEL_DREAM_H
