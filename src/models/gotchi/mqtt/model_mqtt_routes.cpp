#include "model_mqtt_routes.h"
#include "../config/default_config.h"
#include "common/managers/mqtt/mqtt_manager.h"
#include "common/utils/mac_utils.h"
#include "common/managers/sd/sd_manager.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <cstring>

static bool handleGetInfo(const JsonObject& json);

bool ModelGotchiMqttRoutes::processMessage(const JsonObject& json) {
  if (!json["action"].is<const char*>()) {
    Serial.println("[MQTT-ROUTE-GOTCHI] Erreur: action manquante");
    return false;
  }

  const char* action = json["action"].as<const char*>();
  if (action == nullptr) {
    Serial.println("[MQTT-ROUTE-GOTCHI] Erreur: action est null");
    return false;
  }

  if (strcmp(action, "get-info") == 0 || strcmp(action, "getinfo") == 0) {
    return handleGetInfo(json);
  }

  Serial.println("[MQTT-ROUTE-GOTCHI] Action inconnue");
  return false;
}

void ModelGotchiMqttRoutes::printRoutes() {
  Serial.println("\n=== Routes MQTT - Gotchi ===");
  Serial.println("  - get-info");
}

static bool handleGetInfo(const JsonObject& json) {
  (void)json;

  uint64_t totalBytes = 0;
  uint64_t freeBytes = 0;
  uint64_t usedBytes = 0;

  if (SDManager::isAvailable()) {
    totalBytes = SDManager::getTotalSpace();
    usedBytes = SDManager::getUsedSpace();
    freeBytes = SDManager::getFreeSpace();
  }

  char macStr[18];
  if (!getMacAddressString(macStr, sizeof(macStr), ESP_MAC_WIFI_STA)) {
    strcpy(macStr, "00:00:00:00:00:00");
  }

  const char* fwVersion = FIRMWARE_VERSION;

  char infoJson[512];
  snprintf(infoJson, sizeof(infoJson),
    "{"
      "\"type\":\"info\","
      "\"device\":\"Kidoo Gotchi\","
      "\"mac\":\"%s\","
      "\"model\":\"gotchi\","
      "\"firmwareVersion\":\"%s\","
      "\"storage\":{"
        "\"total\":%llu,"
        "\"free\":%llu,"
        "\"used\":%llu"
      "}"
    "}",
    macStr,
    fwVersion,
    totalBytes,
    freeBytes,
    usedBytes
  );

  if (MqttManager::publish(infoJson)) {
    Serial.println("[MQTT-ROUTE-GOTCHI] get-info publié");
    return true;
  }
  Serial.println("[MQTT-ROUTE-GOTCHI] get-info: erreur publication");
  return false;
}
