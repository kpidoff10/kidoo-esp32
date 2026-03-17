#ifndef INIT_MODEL_SOUND_H
#define INIT_MODEL_SOUND_H

#include <Arduino.h>

/**
 * Initialisation spécifique au modèle Kidoo Sound
 */

class InitModelSound {
public:
  static bool init();
  static bool configure();
  static void update();
};

#endif // INIT_MODEL_SOUND_H
