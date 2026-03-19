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

// Tâche FreeRTOS pour config sync commune
#ifdef HAS_WIFI
static void configSyncCommonTask(void* param) {
  Serial.println("[CONFIG-SYNC-COMMON] ========== TASK STARTED ==========");
  bool result = ModelConfigSyncCommon::fetchConfigFromAPI();
  Serial.printf("[CONFIG-SYNC-COMMON] ========== TASK END (result=%d) ==========\n", result ? 1 : 0);
  vTaskDelete(nullptr);
}
#endif

void ModelConfigSyncCommon::fetchAndSaveCmdTokenSecret() {
  Serial.println("[CONFIG-SYNC-COMMON] ========== DEBUT CONFIG SYNC ==========");
  Serial.println("[CONFIG-SYNC-COMMON] Lancement tache pour synchroniser le secret MQTT");

  #ifdef HAS_RTC
  Serial.printf("[CONFIG-SYNC-COMMON] RTC initialized=%d\n", RTCManager::isInitialized() ? 1 : 0);
  if (!RTCManager::isInitialized()) {
    Serial.println("[CONFIG-SYNC-COMMON] RTC pas encore initialise - config-sync differee");
    return;
  }

  // Synchroniser le RTC d'abord (avec NTP) avant les requêtes API signées
  Serial.println("[CONFIG-SYNC-COMMON] autoSyncIfNeeded()");
  RTCManager::autoSyncIfNeeded();
  Serial.println("[CONFIG-SYNC-COMMON] RTC synced");
  #endif

  #ifdef HAS_WIFI
  Serial.println("[CONFIG-SYNC-COMMON] Attendre 1000ms avant config sync");
  // Attendre que WiFi soit complètement stable avant de lancer config sync
  // (évite les race conditions avec d'autres opérations)
  vTaskDelay(pdMS_TO_TICKS(1000));
  Serial.println("[CONFIG-SYNC-COMMON] Création tache FreeRTOS");

  // Créer une tâche FreeRTOS pour ne pas bloquer
  xTaskCreate(
    configSyncCommonTask,
    "ConfigSyncCommon",
    8192,
    nullptr,
    1,
    nullptr
  );
  Serial.println("[CONFIG-SYNC-COMMON] Tache créée");
  #endif
}

bool ModelConfigSyncCommon::fetchConfigFromAPI() {
#ifdef HAS_WIFI
  Serial.println("[CONFIG-SYNC-COMMON] fetchConfigFromAPI() appelée");

  if (!WiFiManager::isConnected()) {
    Serial.println("[CONFIG-SYNC-COMMON] WiFi non connecte");
    return false;
  }
  Serial.println("[CONFIG-SYNC-COMMON] WiFi OK");

#ifdef HAS_RTC
  if (!RTCManager::isAvailable()) {
    Serial.println("[CONFIG-SYNC-COMMON] RTC non disponible");
    return false;
  }
  Serial.println("[CONFIG-SYNC-COMMON] RTC OK");
#endif

  // Obtenir l'adresse MAC WiFi
  char macStr[18];
  Serial.println("[CONFIG-SYNC-COMMON] Etape 1: Recuperation MAC");
  if (!getMacAddressString(macStr, sizeof(macStr), ESP_MAC_WIFI_STA)) {
    Serial.println("[CONFIG-SYNC-COMMON] Erreur lors de la recuperation de l'adresse MAC");
    return false;
  }
  Serial.println("[CONFIG-SYNC-COMMON] MAC reçue");

  // Nettoyer la MAC (supprimer les ':' et '-')
  char macClean[13] = {0};
  for (size_t i = 0, j = 0; macStr[i] && j < sizeof(macClean) - 1; i++) {
    if (macStr[i] != ':' && macStr[i] != '-') {
      macClean[j++] = (macStr[i] >= 'a' && macStr[i] <= 'z') ? macStr[i] - 32 : macStr[i];
    }
  }
  Serial.println("[CONFIG-SYNC-COMMON] MAC nettoyée");

  char path[64];
  snprintf(path, sizeof(path), "/api/devices/%s/config", macClean);

  Serial.print("[CONFIG-SYNC-COMMON] Etape 2: Appel API - ");
  Serial.println(path);

  String payload;
  Serial.println("[CONFIG-SYNC-COMMON] Avant getJsonWithDeviceAuth");
  int httpCode = ApiManager::getJsonWithDeviceAuth(path, &payload, 10000);
  Serial.printf("[CONFIG-SYNC-COMMON] Apres getJsonWithDeviceAuth - code=%d\n", httpCode);

  if (httpCode != 200) {
    Serial.print("[CONFIG-SYNC-COMMON] Erreur HTTP: ");
    Serial.println(httpCode);
    return false;
  }
  Serial.println("[CONFIG-SYNC-COMMON] HTTP 200 OK");

  Serial.println("[CONFIG-SYNC-COMMON] ✓ HTTP 200 - Réponse reçue");

  if (payload.length() == 0) {
    Serial.println("[CONFIG-SYNC-COMMON] Reponse vide");
    return false;
  }

  // Parser le JSON de la réponse
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.print("[CONFIG-SYNC-COMMON] Erreur parsing JSON: ");
    Serial.println(error.c_str());
    return false;
  }

  // Vérifier que la réponse est un succès
  if (!doc["success"].is<bool>() || !doc["success"].as<bool>()) {
    Serial.println("[CONFIG-SYNC-COMMON] Reponse indique un echec");
    return false;
  }

  JsonObject data = doc["data"];
  if (!data) {
    Serial.println("[CONFIG-SYNC-COMMON] Pas de champ 'data' dans la reponse");
    return false;
  }

  // Charger la configuration actuelle depuis la SD
  SDConfig config = SDManager::getConfig();

  // Traiter le cmdTokenSecret s'il est présent dans la réponse
  bool configChanged = false;
  if (data["cmdTokenSecret"].is<const char*>()) {
    const char* cmdTokenSecret = data["cmdTokenSecret"].as<const char*>();
    if (cmdTokenSecret && strlen(cmdTokenSecret) > 0) {
      Serial.printf("[CONFIG-SYNC-COMMON] Secret de token reçu (%u bytes)\n", (unsigned int)strlen(cmdTokenSecret));
      strncpy(config.cmdTokenSecret, cmdTokenSecret, sizeof(config.cmdTokenSecret) - 1);
      config.cmdTokenSecret[sizeof(config.cmdTokenSecret) - 1] = '\0';
      configChanged = true;
    }
  }

  // Sauvegarder la configuration dans la SD si quelque chose a changé
  if (configChanged) {
    if (SDManager::saveConfig(config)) {
      Serial.println("[CONFIG-SYNC-COMMON] Secret MQTT sauvegardé avec succès");
      return true;
    } else {
      Serial.println("[CONFIG-SYNC-COMMON] Erreur lors de la sauvegarde de la configuration");
      return false;
    }
  } else {
    Serial.println("[CONFIG-SYNC-COMMON] Aucun secret MQTT dans la réponse");
    return false;
  }

#else
  return false;
#endif
}
