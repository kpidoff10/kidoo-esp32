#include "../managers/init/init_manager.h"
#include "../managers/audio/audio_manager.h"
#include "../../model_config.h"

/**
 * Initialisation du gestionnaire Audio I2S
 */

#ifdef HAS_AUDIO

bool InitManager::initAudio() {
  systemStatus.audio = INIT_IN_PROGRESS;
  
  Serial.println("[INIT] Initialisation Audio I2S...");
  
  if (AudioManager::init()) {
    systemStatus.audio = INIT_SUCCESS;
    Serial.println("[INIT] Audio I2S OK");
    return true;
  } else {
    systemStatus.audio = INIT_FAILED;
    Serial.println("[INIT] Audio I2S ERREUR");
    return false;
  }
}

#else

bool InitManager::initAudio() {
  systemStatus.audio = INIT_NOT_STARTED;
  return true;
}

#endif
