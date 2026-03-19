#include "model_config_sync_routes.h"

// Config sync commune pour tous les modèles (voir model_config_sync_common.cpp)
// Sound n'a pas de sync spécifique au modèle

void ModelSoundConfigSyncRoutes::onWiFiConnected() {
  // Rien à faire - la synchronisation commune est gérée par ModelConfigSyncCommon
  // qui est appelée automatiquement par WiFiManager
}
