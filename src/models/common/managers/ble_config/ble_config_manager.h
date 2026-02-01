#ifndef BLE_CONFIG_MANAGER_H
#define BLE_CONFIG_MANAGER_H

#include <Arduino.h>

/**
 * Gestionnaire d'activation BLE via bouton
 * 
 * Ce module gère l'activation/désactivation du BLE via un bouton physique.
 * Pour des raisons de sécurité et d'économie d'énergie, le BLE est désactivé
 * par défaut et ne s'active que via un appui long (3 secondes) sur un bouton.
 * 
 * Fonctionnalités:
 * - Détection d'appui long (3 secondes) sur un bouton
 * - Activation du BLE pendant une durée limitée (15 minutes par défaut)
 * - Feedback visuel via les LEDs
 * - Désactivation automatique après timeout
 * - Désactivation manuelle possible
 */

class BLEConfigManager {
public:
  /**
   * Initialiser le gestionnaire
   * @param buttonPin Pin GPIO du bouton (INPUT_PULLUP)
   * @return true si l'initialisation est réussie
   */
  static bool init(uint8_t buttonPin);
  
  /**
   * Vérifier si le gestionnaire est initialisé
   * @return true si initialisé
   */
  static bool isInitialized();
  
  /**
   * Mettre à jour le gestionnaire (à appeler dans loop())
   * Détecte les appuis sur le bouton et gère le timeout
   */
  static void update();
  
  /**
   * Vérifier si le BLE est activé (via bouton)
   * @return true si le BLE est activé
   */
  static bool isBLEEnabled();
  
  /**
   * Activer le BLE manuellement (sans bouton)
   * @param durationMs Durée d'activation en millisecondes (0 = durée par défaut)
   * @param enableFeedback Activer le feedback lumineux (LED) - défaut: true
   * @return true si l'activation est réussie
   */
  static bool enableBLE(uint32_t durationMs = 0, bool enableFeedback = true);
  
  /**
   * Désactiver le BLE manuellement
   */
  static void disableBLE();
  
  /**
   * Obtenir le temps restant avant désactivation automatique
   * @return Temps restant en millisecondes (0 si désactivé)
   */
  static uint32_t getRemainingTime();
  
  /**
   * Définir la durée d'activation par défaut
   * @param durationMs Durée en millisecondes (défaut: 15 minutes)
   */
  static void setDefaultDuration(uint32_t durationMs);
  
  /**
   * Définir la durée d'appui long requise
   * @param durationMs Durée en millisecondes (défaut: 3000ms = 3 secondes)
   */
  static void setLongPressDuration(uint32_t durationMs);
  
  /**
   * Afficher les informations du gestionnaire
   */
  static void printInfo();

private:
  // États du bouton
  enum ButtonState {
    BUTTON_IDLE,           // Bouton relâché
    BUTTON_PRESSED,        // Bouton pressé (en attente de confirmation)
    BUTTON_LONG_PRESS,     // Appui long détecté
    BUTTON_RELEASED        // Bouton relâché après appui long
  };
  
  // Variables statiques
  static bool initialized;
  static uint8_t buttonPin;
  static ButtonState buttonState;
  static unsigned long pressStartTime;
  static unsigned long bleEnableTime;
  static uint32_t bleDuration;
  static uint32_t defaultDuration;
  static uint32_t longPressDuration;
  static bool bleEnabled;
  static bool feedbackActive;
  static bool feedbackEnabled;  // Indique si le feedback était activé au départ
  static unsigned long lastFeedbackTime;
  static unsigned long buttonCooldownUntil;  // Période de refroidissement après appui annulé
  
  // Constantes
  static const uint32_t DEFAULT_BLE_DURATION = 900000;  // 15 minutes
  static const uint32_t DEFAULT_LONG_PRESS = 3000;      // 3 secondes
  static const uint32_t FEEDBACK_INTERVAL = 500;        // 500ms pour clignotement
  static const uint32_t DEBOUNCE_DELAY = 50;            // 50ms anti-rebond
  static const uint32_t COOLDOWN_DELAY = 200;           // 200ms période de refroidissement après appui annulé
  
  // Méthodes privées
  static void handleButtonPress();
  static void handleBLEActivation();
  static void handleBLEDeactivation();
  static void updateFeedback();
  static bool isButtonPressed();
};

#endif // BLE_CONFIG_MANAGER_H
