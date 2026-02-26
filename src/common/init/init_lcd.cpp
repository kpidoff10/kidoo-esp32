#include "common/managers/init/init_manager.h"
#include "models/model_config.h"

#ifdef HAS_LCD
#include "common/managers/lcd/lcd_manager.h"
#endif

bool InitManager::initLCD() {
#ifndef HAS_LCD
  return true;
#else
  if (!HAS_LCD) {
    return true;
  }
  return LCDManager::init();
#endif
}
