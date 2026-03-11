#ifndef COLOR_STATE_H
#define COLOR_STATE_H

#include <stdint.h>

/**
 * État de couleur pour les transitions de fade (WakeupManager).
 *
 * Consolidates 6 variables used for color tracking during fade animations:
 * - startColor: Initial color captured from current LED state
 * - lastColor: Last applied color (for change detection)
 */
struct ColorState {
  // Couleur initiale (capturée au démarrage du fade)
  uint8_t startR = 0;
  uint8_t startG = 0;
  uint8_t startB = 0;

  // Dernière couleur appliquée (pour détection de changement)
  uint8_t lastR = 255;
  uint8_t lastG = 255;
  uint8_t lastB = 255;
};

#endif // COLOR_STATE_H
