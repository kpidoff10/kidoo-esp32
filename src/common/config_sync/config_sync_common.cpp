#include "config_sync_common.h"
#include "common/managers/wifi/wifi_manager.h"
#include "common/managers/sd/sd_manager.h"
#include "common/managers/api/api_manager.h"
#include "common/managers/rtc/rtc_manager.h"
#include "common/utils/mac_utils.h"

#ifdef HAS_WIFI
#include <ArduinoJson.h>
#include <SD.h>
#include <esp_mac.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

#ifdef HAS_WIFI
static void configSyncCommonTask(void* param) {
  bool result = ModelConfigSyncCommon::fetchConfigFromAPI();
  if (!result) {
    Serial.println("[CONFIG-SYNC] ERREUR: fetchConfigFromAPI échoué");
  }
  vTaskDelete(nullptr);
}
#endif

void ModelConfigSyncCommon::fetchAndSaveCmdTokenSecret() {
  #ifdef HAS_RTC
  if (!RTCManager::isInitialized()) {
    Serial.println("[CONFIG-SYNC] ERREUR: RTC pas initialisé");
    return;
  }
  RTCManager::autoSyncIfNeeded();
  #endif

  #ifdef HAS_WIFI
  vTaskDelay(pdMS_TO_TICKS(1000));
  xTaskCreate(configSyncCommonTask, "ConfigSyncCommon", 8192, nullptr, 1, nullptr);
  #endif
}

bool ModelConfigSyncCommon::fetchConfigFromAPI() {
#ifdef HAS_WIFI
  if (!WiFiManager::isConnected()) {
    Serial.println("[CONFIG-SYNC] ERREUR: WiFi non connecté");
    return false;
  }

#ifdef HAS_RTC
  if (!RTCManager::isAvailable()) {
    Serial.println("[CONFIG-SYNC] ERREUR: RTC non disponible");
    return false;
  }
#endif

  char macStr[18];
  if (!getMacAddressString(macStr, sizeof(macStr), ESP_MAC_WIFI_STA)) {
    Serial.println("[CONFIG-SYNC] ERREUR: Récupération MAC échouée");
    return false;
  }

  char macClean[13] = {0};
  for (size_t i = 0, j = 0; macStr[i] && j < sizeof(macClean) - 1; i++) {
    if (macStr[i] != ':' && macStr[i] != '-') {
      macClean[j++] = (macStr[i] >= 'a' && macStr[i] <= 'z') ? macStr[i] - 32 : macStr[i];
    }
  }

  char path[64];
  snprintf(path, sizeof(path), "/api/devices/%s/config", macClean);

  String payload;
  int httpCode = ApiManager::getJsonWithDeviceAuth(path, &payload, 10000);
  if (httpCode != 200) {
    Serial.printf("[CONFIG-SYNC] ERREUR HTTP %d sur %s\n", httpCode, path);
    return false;
  }

  if (payload.length() == 0) {
    Serial.println("[CONFIG-SYNC] ERREUR: Réponse vide");
    return false;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.printf("[CONFIG-SYNC] ERREUR parsing JSON: %s\n", error.c_str());
    return false;
  }

  if (!doc["success"].is<bool>() || !doc["success"].as<bool>()) {
    Serial.println("[CONFIG-SYNC] ERREUR: Réponse indique un échec");
    return false;
  }

  JsonObject data = doc["data"];
  if (!data) {
    Serial.println("[CONFIG-SYNC] ERREUR: Pas de champ 'data'");
    return false;
  }

  SDConfig config = SDManager::getConfig();
  bool configChanged = false;

  if (data["cmdTokenSecret"].is<const char*>()) {
    const char* cmdTokenSecret = data["cmdTokenSecret"].as<const char*>();
    if (cmdTokenSecret && strlen(cmdTokenSecret) > 0) {
      strncpy(config.cmdTokenSecret, cmdTokenSecret, sizeof(config.cmdTokenSecret) - 1);
      config.cmdTokenSecret[sizeof(config.cmdTokenSecret) - 1] = '\0';
      configChanged = true;
    }
  }

  if (configChanged) {
    if (!SDManager::saveConfig(config)) {
      Serial.println("[CONFIG-SYNC] ERREUR: Sauvegarde SD échouée");
      return false;
    }
    return true;
  }

  return false;

#else
  return false;
#endif
}
