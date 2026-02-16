#ifndef VIBRATOR_MANAGER_H
#define VIBRATOR_MANAGER_H

#include <Arduino.h>

/**
 * Gestionnaire du moteur de vibration (PWM)
 *
 * Pilote un module vibreur type "PWM Vibration Motor" branché sur une sortie
 * GPIO PWM (ex. module Arduino UNO/MEGA2560). Permet on/off et intensité 0-255.
 *
 * Activé via HAS_VIBRATOR dans la config du modèle.
 * Pin configurée via VIBRATOR_PIN (défaut 25 si non défini).
 */

#ifdef HAS_VIBRATOR

class VibratorManager {
public:
  /**
   * Initialiser le gestionnaire vibreur (GPIO + PWM)
   * @return true si l'initialisation a réussi
   */
  static bool init();

  /**
   * Vérifier si le vibreur est initialisé et disponible
   */
  static bool isInitialized();

  /**
   * Activer ou désactiver la vibration
   * @param on true = vibre, false = arrêt
   */
  static void setOn(bool on);

  /**
   * Définir l'intensité (PWM 0-255). 0 = arrêt, 255 = max.
   * @param value 0-255
   */
  static void setIntensity(uint8_t value);

  /**
   * Obtenir l'intensité actuelle (0-255)
   */
  static uint8_t getIntensity();

  /**
   * Vibration en cours (pin à un niveau non nul)
   */
  static bool isOn();

  /**
   * Impulsion : vibrer pendant durationMs puis arrêter
   * @param durationMs durée en millisecondes
   * @param intensity intensité 0-255 (défaut 255)
   */
  static void pulse(uint32_t durationMs, uint8_t intensity = 255);

  /**
   * Arrêter immédiatement la vibration
   */
  static void stop();

  /** Types d'effets prédéfinis */
  enum Effect {
    EFFECT_SHORT,      // Court : une brève impulsion
    EFFECT_LONG,       // Long : vibration prolongée
    EFFECT_JERKY,      // Saccadé : plusieurs micro-impulsions
    EFFECT_PULSE,      // Pulsation : on/off répété
    EFFECT_DOUBLE_TAP  // Double tap : toc-toc
  };

  /**
   * Jouer un effet prédéfini (bloquant pendant la durée de l'effet)
   * @param effect type d'effet
   * @return true si l'effet a été joué
   */
  static bool playEffect(Effect effect);

  /**
   * Afficher le statut sur Serial (pour debug / commandes)
   */
  static void printStatus();

private:
  static bool initialized;
  static uint8_t currentIntensity;
  static bool currentOn;
};

#endif // HAS_VIBRATOR

#endif // VIBRATOR_MANAGER_H
