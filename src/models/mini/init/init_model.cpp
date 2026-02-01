#include "init_model.h"
#include "../../common/managers/init/init_manager.h"

/**
 * Initialisation spécifique au modèle Kidoo Mini
 */

bool InitModelMini::configure() {
  // Configuration spécifique au Mini avant l'initialisation
  // Exemple : configurer des pins spécifiques, des paramètres, etc.
  
  Serial.println("[INIT] Configuration modele Mini");
  
  // Ajouter ici toute configuration spécifique au Mini
  // Par exemple :
  // - Configurer des pins différents
  // - Définir des paramètres spécifiques au Mini
  // - Initialiser des composants uniquement présents sur le Mini
  
  return true;
}

bool InitModelMini::init() {
  // Initialisation spécifique au Mini après l'initialisation des composants communs
  // Exemple : initialiser des composants supplémentaires, calibrer, etc.
  
  Serial.println("[INIT] Initialisation modele Mini");
  
  // Ajouter ici toute initialisation spécifique au Mini
  // Par exemple :
  // - Initialiser des capteurs différents
  // - Configurer des périphériques spécifiques au Mini
  // - Effectuer des calibrations spécifiques
  
  return true;
}
