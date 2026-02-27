#ifndef DREAM_TOUCH_HANDLER_H
#define DREAM_TOUCH_HANDLER_H

#include <Arduino.h>

/**
 * Gestionnaire du touch (TTP223) pour le modèle Dream
 *
 * Gestuelle :
 * - Tap court (< 1 s) : alerte veilleuse si activée (notification aux parents)
 * - Main posée 3 s : arrêter ou lancer la routine (bedtime/wakeup)
 *
 * Feedback lumineux :
 * - Vert pulsé 2 s après envoi alerte = message envoyé
 * - Rouge pulsé 3 s = pas de routine configurée pour aujourd'hui
 */

class DreamTouchHandler {
public:
  /**
   * Mettre à jour le handler (à appeler dans loop() quand HAS_TOUCH et KIDOO_MODEL_DREAM)
   */
  static void update();
};

#endif // DREAM_TOUCH_HANDLER_H
