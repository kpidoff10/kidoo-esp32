#include "model_config_sync_routes.h"
#include "../../common/managers/wifi/wifi_manager.h"
#include "../../common/managers/sd/sd_manager.h"
#include "../../common/config/default_config.h"
#include "../../common/utils/mac_utils.h"
#include "../managers/bedtime/bedtime_manager.h"
#include "../managers/wakeup/wakeup_manager.h"

#ifdef HAS_WIFI
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_mac.h>  // Pour ESP_MAC_WIFI_STA
#endif

void ModelDreamConfigSyncRoutes::onWiFiConnected() {
  Serial.println("[CONFIG-SYNC] WiFi connecte - Recuperation de la configuration depuis l'API");
  fetchConfigFromAPI();
}

bool ModelDreamConfigSyncRoutes::fetchConfigFromAPI() {
#ifdef HAS_WIFI
  if (!WiFiManager::isConnected()) {
    Serial.println("[CONFIG-SYNC] WiFi non connecte, impossible de recuperer la configuration");
    return false;
  }

  // Obtenir l'adresse MAC WiFi formatée
  char macStr[18];
  if (!getMacAddressString(macStr, sizeof(macStr), ESP_MAC_WIFI_STA)) {
    Serial.println("[CONFIG-SYNC] Erreur lors de la recuperation de l'adresse MAC");
    return false;
  }

  Serial.print("[CONFIG-SYNC] Adresse MAC: ");
  Serial.println(macStr);

  // Construire l'URL de l'API
  char url[256];
  snprintf(url, sizeof(url), "%s/api/kidoos/config/%s", API_BASE_URL, macStr);

  Serial.print("[CONFIG-SYNC] URL: ");
  Serial.println(url);

  HTTPClient http;
  http.begin(url);
  http.setConnectTimeout(5000);
  http.setTimeout(10000);

  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.print("[CONFIG-SYNC] Erreur HTTP: ");
    Serial.println(httpCode);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  if (payload.length() == 0) {
    Serial.println("[CONFIG-SYNC] Reponse vide");
    return false;
  }

  Serial.print("[CONFIG-SYNC] Reponse recue (");
  Serial.print(payload.length());
  Serial.println(" bytes)");

  // Parser le JSON de la réponse
  // Format attendu: {"success": true, "data": {"bedtime": {...}, "wakeup": {...}}}
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  StaticJsonDocument<2048> doc;
  #pragma GCC diagnostic pop

  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.print("[CONFIG-SYNC] Erreur parsing JSON: ");
    Serial.println(error.c_str());
    return false;
  }

  // Vérifier que la réponse est un succès
  if (!doc["success"].is<bool>() || !doc["success"].as<bool>()) {
    Serial.println("[CONFIG-SYNC] Reponse indique un echec");
    return false;
  }

  JsonObject data = doc["data"];
  if (!data) {
    Serial.println("[CONFIG-SYNC] Pas de champ 'data' dans la reponse");
    return false;
  }

  // Charger la configuration actuelle depuis la SD
  SDConfig config = SDManager::getConfig();

  // Mettre à jour la configuration bedtime si présente
  if (data["bedtime"].is<JsonObject>()) {
    JsonObject bedtime = data["bedtime"];

    if (bedtime["colorR"].is<int>()) {
      config.bedtime_colorR = (uint8_t)bedtime["colorR"].as<int>();
    }
    if (bedtime["colorG"].is<int>()) {
      config.bedtime_colorG = (uint8_t)bedtime["colorG"].as<int>();
    }
    if (bedtime["colorB"].is<int>()) {
      config.bedtime_colorB = (uint8_t)bedtime["colorB"].as<int>();
    }
    if (bedtime["brightness"].is<int>()) {
      int brightness = bedtime["brightness"].as<int>();
      if (brightness >= 0 && brightness <= 100) {
        config.bedtime_brightness = (uint8_t)brightness;
      }
    }
    if (bedtime["nightlightAllNight"].is<bool>()) {
      config.bedtime_allNight = bedtime["nightlightAllNight"].as<bool>();
    }

    // Mettre à jour weekdaySchedule
    if (bedtime["weekdaySchedule"].is<JsonObject>()) {
      String scheduleStr;
      serializeJson(bedtime["weekdaySchedule"], scheduleStr);
      strncpy(config.bedtime_weekdaySchedule, scheduleStr.c_str(), sizeof(config.bedtime_weekdaySchedule) - 1);
      config.bedtime_weekdaySchedule[sizeof(config.bedtime_weekdaySchedule) - 1] = '\0';
    } else if (bedtime["weekdaySchedule"].isNull()) {
      strcpy(config.bedtime_weekdaySchedule, "{}");
    }

    Serial.println("[CONFIG-SYNC] Configuration bedtime mise a jour");
  }

  // Mettre à jour la configuration wakeup si présente
  if (data["wakeup"].is<JsonObject>()) {
    JsonObject wakeup = data["wakeup"];

    if (wakeup["colorR"].is<int>()) {
      config.wakeup_colorR = (uint8_t)wakeup["colorR"].as<int>();
    }
    if (wakeup["colorG"].is<int>()) {
      config.wakeup_colorG = (uint8_t)wakeup["colorG"].as<int>();
    }
    if (wakeup["colorB"].is<int>()) {
      config.wakeup_colorB = (uint8_t)wakeup["colorB"].as<int>();
    }
    if (wakeup["brightness"].is<int>()) {
      int brightness = wakeup["brightness"].as<int>();
      if (brightness >= 0 && brightness <= 100) {
        config.wakeup_brightness = (uint8_t)brightness;
      }
    }

    // Mettre à jour weekdaySchedule
    if (wakeup["weekdaySchedule"].is<JsonObject>()) {
      String scheduleStr;
      serializeJson(wakeup["weekdaySchedule"], scheduleStr);
      strncpy(config.wakeup_weekdaySchedule, scheduleStr.c_str(), sizeof(config.wakeup_weekdaySchedule) - 1);
      config.wakeup_weekdaySchedule[sizeof(config.wakeup_weekdaySchedule) - 1] = '\0';
    } else if (wakeup["weekdaySchedule"].isNull()) {
      strcpy(config.wakeup_weekdaySchedule, "{}");
    }

    Serial.println("[CONFIG-SYNC] Configuration wakeup mise a jour");
  }

  // Sauvegarder la configuration mise à jour dans la SD
  if (SDManager::saveConfig(config)) {
    Serial.println("[CONFIG-SYNC] Configuration sauvegardee dans la SD");

    // Recharger les configurations dans les managers
    BedtimeManager::reloadConfig();
    WakeupManager::reloadConfig();

    return true;
  } else {
    Serial.println("[CONFIG-SYNC] Erreur lors de la sauvegarde de la configuration");
    return false;
  }
#else
  return false;
#endif
}
