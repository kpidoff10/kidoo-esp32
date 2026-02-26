#ifndef TOUCH_MANAGER_H
#define TOUCH_MANAGER_H

#include <Arduino.h>

/**
 * Gestionnaire du capteur tactile TTP223 (TTP223B)
 *
 * Module 1 canal : sortie digitale HIGH quand on touche, LOW sinon.
 * Lecture avec debounce pour éviter les rebonds.
 *
 * Activé via HAS_TOUCH dans la config du modèle.
 * Pin configurée via TOUCH_PIN.
 */

#ifdef HAS_TOUCH

class TouchManager {
public:
  /**
   * Initialiser le gestionnaire (GPIO en entrée)
   * @return true si l'initialisation a réussi
   */
  static bool init();

  /**
   * Vérifier si le touch est initialisé et disponible
   */
  static bool isInitialized();

  /**
   * Mettre à jour l'état (debounce). À appeler régulièrement dans loop().
   */
  static void update();

  /**
   * État tactile debounced : true = touché, false = relâché
   */
  static bool isTouched();

  /**
   * Lecture brute de la broche (sans debounce)
   */
  static bool readRaw();

  /**
   * Définir le temps de debounce en ms (défaut 50)
   */
  static void setDebounceMs(uint32_t ms);

  /**
   * Afficher le statut sur Serial (pour debug / commandes)
   */
  static void printStatus();

private:
  static bool initialized;
  static bool debouncedState;
  static bool lastRawState;
  static uint32_t lastChangeTime;
  static uint32_t debounceMs;
};

#endif // HAS_TOUCH

#endif // TOUCH_MANAGER_H
