#include "init_model.h"
#include "common/managers/init/init_manager.h"
#include "common/managers/wifi/wifi_manager.h"
#include "models/model_pubnub_routes.h"
#include "models/dream/pubnub/model_pubnub_routes.h"
#include "models/dream/config_sync/model_config_sync_routes.h"
#include "models/dream/managers/bedtime/bedtime_manager.h"
#include "models/dream/managers/wakeup/wakeup_manager.h"
#include "models/dream/managers/touch/dream_touch_handler.h"

/**
 * Initialisation spécifique au modèle Kidoo Dream
 */

bool InitModelDream::configure() {
  // Configuration spécifique au Dream avant l'initialisation
  return true;
}

bool InitModelDream::init() {
  // Initialisation spécifique au Dream après l'initialisation des composants communs
  // Note: Dream n'a pas de NFC, donc pas d'initialisation du handler NFC

  // Réinitialiser les flags de test au démarrage
  ModelDreamPubNubRoutes::resetTestFlags();

  // Initialiser le gestionnaire bedtime automatique
  if (!BedtimeManager::init()) {
    Serial.println("[INIT-DREAM] ERREUR: Echec initialisation BedtimeManager");
    // Ne pas bloquer l'initialisation si le bedtime échoue
  }

  // Initialiser le gestionnaire wake-up automatique
  if (!WakeupManager::init()) {
    Serial.println("[INIT-DREAM] ERREUR: Echec initialisation WakeupManager");
    // Ne pas bloquer l'initialisation si le wake-up échoue
  }

  // Si WiFi est connecté et config-sync était différée (RTC non prêt), le relancer maintenant
  // (RTC est maintenant initialisé)
#ifdef HAS_WIFI
  if (WiFiManager::isConnected()) {
    Serial.println("[INIT-DREAM] RTC prêt - Relancer config-sync différée");
    ModelDreamConfigSyncRoutes::retryFetchConfig();
  }
#endif

  return true;
}

void InitModelDream::update() {
  ModelPubNubRoutes::checkTestBedtimeTimeout();
  ModelPubNubRoutes::checkTestWakeupTimeout();
  ModelPubNubRoutes::checkTestDefaultConfigTimeout();
  ModelPubNubRoutes::checkNighttimeAlertAckTimeout();
  ModelPubNubRoutes::updateEnvPublisher();

  BedtimeManager::update();
  WakeupManager::update();

#ifdef HAS_TOUCH
  if (HAS_TOUCH) {
    DreamTouchHandler::update();
  }
#endif
}
