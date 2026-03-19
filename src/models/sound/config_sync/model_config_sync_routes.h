#ifndef MODEL_SOUND_CONFIG_SYNC_ROUTES_H
#define MODEL_SOUND_CONFIG_SYNC_ROUTES_H

#include <Arduino.h>

/**
 * Routes de synchronisation de configuration pour Sound
 */

class ModelSoundConfigSyncRoutes {
public:
  // Config sync commune pour tous les modèles (voir model_config_sync_common.cpp)
  static void onWiFiConnected();
};

#endif // MODEL_SOUND_CONFIG_SYNC_ROUTES_H
