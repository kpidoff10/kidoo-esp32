#include "../managers/init/init_manager.h"
#include "../../model_config.h"
#ifdef HAS_TOUCH
#include "../managers/touch/touch_manager.h"
#endif

/**
 * Initialisation du capteur tactile TTP223
 */

#ifdef HAS_TOUCH

bool InitManager::initTouch() {
  systemStatus.touch = INIT_IN_PROGRESS;

  if (!HAS_TOUCH) {
    systemStatus.touch = INIT_NOT_STARTED;
    return true;
  }

  if (TouchManager::init()) {
    systemStatus.touch = INIT_SUCCESS;
    return true;
  }

  systemStatus.touch = INIT_FAILED;
  return false;
}

#else

bool InitManager::initTouch() {
  systemStatus.touch = INIT_NOT_STARTED;
  return true;
}

#endif
