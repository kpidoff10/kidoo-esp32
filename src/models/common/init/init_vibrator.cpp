#include "../managers/init/init_manager.h"
#include "../../model_config.h"
#ifdef HAS_VIBRATOR
#include "../managers/vibrator/vibrator_manager.h"
#endif

/**
 * Initialisation du gestionnaire Vibreur (PWM)
 */

#ifdef HAS_VIBRATOR

bool InitManager::initVibrator() {
  systemStatus.vibrator = INIT_IN_PROGRESS;

  if (!HAS_VIBRATOR) {
    systemStatus.vibrator = INIT_NOT_STARTED;
    return true;
  }

  if (VibratorManager::init()) {
    systemStatus.vibrator = INIT_SUCCESS;
    return true;
  }

  systemStatus.vibrator = INIT_FAILED;
  return false;
}

#else

bool InitManager::initVibrator() {
  systemStatus.vibrator = INIT_NOT_STARTED;
  return true;
}

#endif
