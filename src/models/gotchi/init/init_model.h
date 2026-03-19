#ifndef INIT_MODEL_GOTCHI_H
#define INIT_MODEL_GOTCHI_H

#include <Arduino.h>

/**
 * Initialisation spécifique au modèle Kidoo Gotchi (Waveshare ESP32-S3 AMOLED)
 */

class InitModelGotchi {
public:
  static bool init();
  static bool configure();
  static void update();
};

#endif // INIT_MODEL_GOTCHI_H
