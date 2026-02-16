#ifndef INIT_MODEL_GOTCHI_H
#define INIT_MODEL_GOTCHI_H

#include <Arduino.h>

/**
 * Initialisation spécifique au modèle Kidoo Gotchi (ESP32-S3-N16R8)
 */

class InitModelGotchi {
public:
  static bool init();
  static bool configure();
  /** Affiche l'écran de démarrage (Kidoo Gotchi Demarrage...) - appelé après re-init LCD différée */
  static void showStartupScreen();
};

#endif // INIT_MODEL_GOTCHI_H
