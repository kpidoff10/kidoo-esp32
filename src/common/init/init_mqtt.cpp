#include "common/managers/init/init_manager.h"
#include "common/managers/mqtt/mqtt_manager.h"
#include "common/managers/wifi/wifi_manager.h"
#include "models/model_config.h"

/**
 * Initialisation du gestionnaire MQTT
 *
 * Cette fonction initialise le client MQTT et tente de se connecter
 */
bool InitManager::initMqtt() {
#ifndef HAS_MQTT
  // MQTT non disponible sur ce modèle
  systemStatus.mqtt = INIT_NOT_STARTED;
  return true;
#else
  if (!HAS_MQTT) {
    systemStatus.mqtt = INIT_NOT_STARTED;
    return true;
  }
  systemStatus.mqtt = INIT_IN_PROGRESS;
  #if ENABLE_VERBOSE_LOGS
  Serial.println("[INIT] Initialisation MQTT...");
  #endif

  // Vérifier que le broker est configuré
  if (strlen(DEFAULT_MQTT_BROKER_HOST) == 0) {
    Serial.println("[INIT] MQTT: Broker non configure dans default_config.h");
    systemStatus.mqtt = INIT_FAILED;
    return false;
  }

  // Initialiser le client MQTT (même si WiFi n'est pas connecté)
  // Le thread se connectera automatiquement quand le WiFi sera disponible
  if (!MqttManager::init()) {
    Serial.println("[INIT] MQTT: Echec initialisation");
    systemStatus.mqtt = INIT_FAILED;
    return false;
  }

  // Si le WiFi est déjà connecté, tenter de se connecter immédiatement
  if (WiFiManager::isConnected()) {
    if (MqttManager::connect()) {
      Serial.println("[INIT] MQTT: Connecte");
      systemStatus.mqtt = INIT_SUCCESS;
      return true;
    } else {
      Serial.println("[INIT] MQTT: Echec connexion (retry auto)");
      systemStatus.mqtt = INIT_SUCCESS; // On considère OK car retry auto
      return true;
    }
  } else {
    // WiFi pas encore connecté, mais MQTT est initialisé
    // Il se connectera automatiquement quand le WiFi sera disponible
    Serial.println("[INIT] MQTT: Initialise (en attente WiFi)");
    systemStatus.mqtt = INIT_SUCCESS;
    return true;
  }
#endif
}
