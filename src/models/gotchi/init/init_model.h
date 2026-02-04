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
};

#endif // INIT_MODEL_GOTCHI_H
