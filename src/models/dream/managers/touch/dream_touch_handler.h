#ifndef DREAM_TOUCH_HANDLER_H
#define DREAM_TOUCH_HANDLER_H

#include <Arduino.h>

/**
 * Gestionnaire du touch (TTP223) pour le modèle Dream
 *
 * Gestuelle :
 * - Tout touche (50ms à 2s) : démarre ou arrête la routine coucher/réveil
 * - Appui maintenu 2s+ (sans relâcher) : envoi alerte veilleuse (notification aux parents)
 *
 * Feedback lumineux :
 * - Vert/rouge pulsé 3s après envoi alerte, puis reprise du mode actuel (bedtime/wakeup)
 * - Rouge pulsé 3s = pas de routine configurée pour aujourd'hui
 */

class DreamTouchHandler {
public:
  /** Timer pour le feedback alerte (utilisé par update() et triggerAlertFeedback) */
  static unsigned long s_alertFeedbackUntil;
  /**
   * Mettre à jour le handler (à appeler dans loop() quand HAS_TOUCH et KIDOO_MODEL_DREAM)
   */
  static void update();

  /**
   * Déclencher le feedback LED après envoi alerte (vert=ok, rouge=échec)
   */
  static void triggerAlertFeedback(bool success);

  /**
   * Vérifier si la couleur par défaut est actuellement affichée (tapotage sans routine)
   * @return true si la couleur par défaut est active, false sinon
   */
  static bool isDefaultColorDisplayed();

  /**
   * Simuler un tap sur le capteur (appelé par PubNub tap-sensor)
   * Applique la même logique qu'un tap physique au relâchement:
   * - Si bedtime actif → arrêter
   * - Si wakeup actif → arrêter
   * - Sinon → démarrer bedtime ou toggle couleur par défaut
   */
  static void simulateTap();
};

#endif // DREAM_TOUCH_HANDLER_H
