#include "model_config_sync_routes.h"
#include "common/managers/wifi/wifi_manager.h"
#include "common/managers/sd/sd_manager.h"
#include "common/managers/api/api_manager.h"
#include "common/managers/rtc/rtc_manager.h"
#include "common/managers/timezone/timezone_manager.h"
#include "common/config/default_config.h"
#include "common/utils/mac_utils.h"
#include "../config/dream_config.h"
#include "models/dream/managers/bedtime/bedtime_manager.h"
#include "models/dream/managers/wakeup/wakeup_manager.h"

#ifdef HAS_WIFI
#include <ArduinoJson.h>
#include <SD.h>
#include <esp_mac.h>  // Pour ESP_MAC_WIFI_STA
#endif

/**
 * Helper pour parser la couleur (R, G, B) depuis un JsonObject
 * Valide et met à jour les références si présentes dans le JSON
 */
static inline void updateColorFromJson(const JsonObject& json, uint8_t& r, uint8_t& g, uint8_t& b) {
  if (json["colorR"].is<int>()) {
    r = (uint8_t)json["colorR"].as<int>();
  }
  if (json["colorG"].is<int>()) {
    g = (uint8_t)json["colorG"].as<int>();
  }
  if (json["colorB"].is<int>()) {
    b = (uint8_t)json["colorB"].as<int>();
  }
}

/**
 * Helper pour parser la brightness (0-100) depuis un JsonObject
 * Retourne la brightness validée, ou laisse la valeur existante inchangée
 */
static inline void updateBrightnessFromJson(const JsonObject& json, uint8_t& brightness) {
  if (json["brightness"].is<int>()) {
    int value = json["brightness"].as<int>();
    if (value >= 0 && value <= 100) {
      brightness = (uint8_t)value;
    }
  }
}

void ModelDreamConfigSyncRoutes::onWiFiConnected() {
  Serial.println("[CONFIG-SYNC] WiFi connecte - Recuperation de la configuration depuis l'API");

  // IMPORTANT: Vérifier que le RTC est initialisé avant de faire les requêtes signées
  // (callback WiFi peut s'exécuter avant l'init complète du système)
  #ifdef HAS_RTC
  if (!RTCManager::isInitialized()) {
    Serial.println("[CONFIG-SYNC] RTC pas encore initialise - config-sync differee");
    return;
  }

  // Synchroniser le RTC d'abord (avec NTP) avant les requêtes API signées
  // Sinon la signature sera invalide si le RTC n'a pas l'heure exacte
  RTCManager::autoSyncIfNeeded();
  #endif

  // Essayer de récupérer et appliquer le fuseau horaire en premier (avant les configs)
  fetchAndApplyTimezoneFromAPI();
  // Puis récupérer les configurations bedtime/wakeup
  fetchConfigFromAPI();
}

void ModelDreamConfigSyncRoutes::retryFetchConfig() {
  Serial.println("[CONFIG-SYNC] Retry avec signature (RTC disponible)");
  fetchConfigFromAPI();
}

bool ModelDreamConfigSyncRoutes::fetchConfigFromAPI() {
#ifdef HAS_WIFI
  if (!WiFiManager::isConnected()) {
    Serial.println("[CONFIG-SYNC] WiFi non connecte, impossible de recuperer la configuration");
    return false;
  }

#ifdef HAS_RTC
  // Ne pas faire config-sync si RTC n'est pas synced (attendre que RTC soit prêt)
  if (!RTCManager::isAvailable()) {
    Serial.println("[CONFIG-SYNC] RTC non disponible - requete differee");
    return false;
  }
#endif

  // Obtenir l'adresse MAC WiFi (format AABBCCDDEEFF pour l'URL)
  char macStr[18];
  if (!getMacAddressString(macStr, sizeof(macStr), ESP_MAC_WIFI_STA)) {
    Serial.println("[CONFIG-SYNC] Erreur lors de la recuperation de l'adresse MAC");
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

  Serial.print("[CONFIG-SYNC] MAC: ");
  Serial.print(macClean);
  Serial.print(" - Requete: ");
  Serial.println(path);

  String payload;
  int httpCode = ApiManager::getJsonWithDeviceAuth(path, &payload, 10000);

  if (httpCode != 200) {
    Serial.print("[CONFIG-SYNC] Erreur HTTP: ");
    Serial.println(httpCode);
    return false;
  }

  Serial.println("[CONFIG-SYNC] ✓ HTTP 200 - Réponse reçue");

  if (payload.length() == 0) {
    Serial.println("[CONFIG-SYNC] Reponse vide");
    return false;
  }

  #if ENABLE_VERBOSE_LOGS
  Serial.print("[CONFIG-SYNC] Reponse recue (");
  Serial.print(payload.length());
  Serial.println(" bytes)");
  #endif

  // Parser le JSON de la réponse
  // Format attendu: {"success": true, "data": {"bedtime": {...}, "wakeup": {...}, "defaultColor": {...}}}
  // Utiliser JsonDocument pour éviter débordement de pile
  JsonDocument doc;
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

  // IMPORTANT: Ne jamais écraser une config SD valide par une config API vide/zéros.
  // Si l'API retourne des zéros ou une config vide (ex: device non configuré côté serveur),
  // on conserve la config existante sur la SD.
  auto isApiConfigMeaningful = [](JsonObject obj, const char* colorKeyR, const char* colorKeyG, const char* colorKeyB, const char* brightnessKey) -> bool {
    int r = obj[colorKeyR].as<int>();
    int g = obj[colorKeyG].as<int>();
    int b = obj[colorKeyB].as<int>();
    int brightness = obj[brightnessKey].as<int>();
    if (r != 0 || g != 0 || b != 0 || brightness > 0) return true;
    if (obj["weekdaySchedule"].is<JsonObject>()) {
      JsonObject sched = obj["weekdaySchedule"];
      for (JsonPair p : sched) {
        if (p.value().is<JsonObject>() && p.value()["activated"].is<bool>() && p.value()["activated"].as<bool>()) return true;
      }
    }
    return false;
  };

  // Mettre à jour la configuration bedtime si présente ET si l'API fournit des données significatives
  if (data["bedtime"].is<JsonObject>()) {
    JsonObject bedtime = data["bedtime"];
    bool shouldMergeBedtime = isApiConfigMeaningful(bedtime, "colorR", "colorG", "colorB", "brightness") || !config.valid;
    if (!shouldMergeBedtime) {
      Serial.println("[CONFIG-SYNC] API bedtime vide/zeros - conservation config SD existante");
    } else {
      updateColorFromJson(bedtime, config.bedtime_colorR, config.bedtime_colorG, config.bedtime_colorB);
      updateBrightnessFromJson(bedtime, config.bedtime_brightness);
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
    }
  }

  // Mettre à jour la configuration wakeup si présente ET si l'API fournit des données significatives
  if (data["wakeup"].is<JsonObject>()) {
    JsonObject wakeup = data["wakeup"];
    bool shouldMergeWakeup = isApiConfigMeaningful(wakeup, "colorR", "colorG", "colorB", "brightness") || !config.valid;
    if (!shouldMergeWakeup) {
      Serial.println("[CONFIG-SYNC] API wakeup vide/zeros - conservation config SD existante");
    } else {
      updateColorFromJson(wakeup, config.wakeup_colorR, config.wakeup_colorG, config.wakeup_colorB);
      updateBrightnessFromJson(wakeup, config.wakeup_brightness);

      // Mettre à jour weekdaySchedule
      if (wakeup["weekdaySchedule"].is<JsonObject>()) {
        String scheduleStr;
        serializeJson(wakeup["weekdaySchedule"], scheduleStr);
        strncpy(config.wakeup_weekdaySchedule, scheduleStr.c_str(), sizeof(config.wakeup_weekdaySchedule) - 1);
        config.wakeup_weekdaySchedule[sizeof(config.wakeup_weekdaySchedule) - 1] = '\0';
      } else if (wakeup["weekdaySchedule"].isNull()) {
        strcpy(config.wakeup_weekdaySchedule, "{}");
      }
    }
  }

  // Sauvegarder la configuration commune (bedtime, wakeup) dans la SD
  if (SDManager::saveConfig(config)) {
    // Config Dream spécifique : alerte réveil nocturne (stockée sous clé "dream")
    if (data["nighttimeAlertEnabled"].is<bool>()) {
      DreamConfig dreamConfig = DreamConfigManager::getConfig();
      dreamConfig.nighttime_alert_enabled = data["nighttimeAlertEnabled"].as<bool>();
      DreamConfigManager::saveConfig(dreamConfig);
    }

    // Mettre à jour la config couleur par défaut si présente
    if (!data["defaultColor"].isNull() && data["defaultColor"].is<JsonObject>()) {
      JsonObject defaultColor = data["defaultColor"];
      DreamConfig dreamConfig = DreamConfigManager::getConfig();
      updateColorFromJson(defaultColor, dreamConfig.default_color_r, dreamConfig.default_color_g, dreamConfig.default_color_b);
      updateBrightnessFromJson(defaultColor, dreamConfig.default_brightness);
      if (defaultColor["effect"].is<const char*>()) {
        const char* effectStr = defaultColor["effect"].as<const char*>();
        strncpy(dreamConfig.default_effect, effectStr, sizeof(dreamConfig.default_effect) - 1);
        dreamConfig.default_effect[sizeof(dreamConfig.default_effect) - 1] = '\0';
      }
      DreamConfigManager::saveConfig(dreamConfig);
      Serial.println("[CONFIG-SYNC] Config couleur par defaut mise a jour depuis /config");
    }

    // Traiter le timezoneId s'il est présent dans la réponse
    if (doc["data"]["timezoneId"].is<const char*>()) {
      const char* timezoneId = doc["data"]["timezoneId"].as<const char*>();
      if (timezoneId && strlen(timezoneId) > 0) {
        Serial.printf("[CONFIG-SYNC] Fuseau horaire reçu: %s\n", timezoneId);

        // Sauvegarder dans config.json (créer si nécessaire)
        if (SDManager::isAvailable()) {
          File configFile = SD.open("/config.json", FILE_READ);

          // Utiliser JsonDocument pour éviter débordement de pile
          JsonDocument configDoc;
          bool shouldSave = false;

          if (configFile) {
            // Fichier existe: charger, mettre à jour
            const size_t maxSize = 512;
            char jsonBuffer[512];
            size_t fileSize = configFile.size();
            if (fileSize > 0 && fileSize < maxSize) {
              size_t bytesRead = configFile.readBytes(jsonBuffer, maxSize - 1);
              jsonBuffer[bytesRead] = '\0';
              if (!deserializeJson(configDoc, jsonBuffer)) {
                shouldSave = true;  // Désérialisation réussie
              }
            }
            configFile.close();
          } else {
            // Fichier n'existe pas: créer un nouveau document
            shouldSave = true;
          }

          if (shouldSave) {
            // Mettre à jour timezoneId
            configDoc["timezoneId"] = timezoneId;

            // Sauvegarder
            configFile = SD.open("/config.json", FILE_WRITE);
            if (configFile) {
              serializeJson(configDoc, configFile);
              configFile.close();
              RTCManager::setTimezoneId(timezoneId);
              Serial.println("[CONFIG-SYNC] timezoneId sauvegardé dans config.json");
            }
          }
        }
      }
    }

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

bool ModelDreamConfigSyncRoutes::fetchAndApplyTimezoneFromAPI() {
#ifdef HAS_WIFI
  if (!WiFiManager::isConnected()) {
    Serial.println("[CONFIG-SYNC] WiFi non connecte, impossible de recuperer le fuseau horaire");
    return false;
  }

#ifdef HAS_RTC
  if (!RTCManager::isAvailable()) {
    Serial.println("[CONFIG-SYNC] RTC non disponible - fuseau horaire non synchronisé");
    return false;
  }
#endif

  // Obtenir l'adresse MAC WiFi (format AABBCCDDEEFF pour l'URL)
  char macStr[18];
  if (!getMacAddressString(macStr, sizeof(macStr), ESP_MAC_WIFI_STA)) {
    Serial.println("[CONFIG-SYNC] Erreur lors de la recuperation de l'adresse MAC (timezone)");
    return false;
  }
  char macClean[13] = {0};
  for (size_t i = 0, j = 0; macStr[i] && j < sizeof(macClean) - 1; i++) {
    if (macStr[i] != ':' && macStr[i] != '-') {
      macClean[j++] = (macStr[i] >= 'a' && macStr[i] <= 'z') ? macStr[i] - 32 : macStr[i];
    }
  }

  char path[64];
  snprintf(path, sizeof(path), "/api/devices/%s/timezone", macClean);

  String payload;
  int httpCode = ApiManager::getJsonWithDeviceAuth(path, &payload, 5000);

  if (httpCode != 200) {
    Serial.print("[CONFIG-SYNC] Erreur HTTP fuseau horaire: ");
    Serial.println(httpCode);
    return false;
  }

  Serial.println("[CONFIG-SYNC] ✓ HTTP 200 - Fuseau horaire reçu");

  if (payload.length() == 0) {
    Serial.println("[CONFIG-SYNC] Reponse fuseau horaire vide");
    return false;
  }

  // Parser le JSON de la réponse
  // Utiliser JsonDocument pour éviter débordement de pile
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.print("[CONFIG-SYNC] Erreur parsing JSON fuseau horaire: ");
    Serial.println(error.c_str());
    return false;
  }

  // Extraire le timezoneId
  if (!doc["timezoneId"].is<const char*>()) {
    Serial.println("[CONFIG-SYNC] timezoneId manquant dans la reponse");
    return false;
  }

  const char* timezoneId = doc["timezoneId"].as<const char*>();
  if (!timezoneId || strlen(timezoneId) == 0) {
    Serial.println("[CONFIG-SYNC] timezoneId vide");
    return false;
  }

  Serial.printf("[CONFIG-SYNC] Fuseau horaire: %s\n", timezoneId);

  // Sauvegarder timezoneId dans config.json pour RTCManager::getLocalDateTime()
  if (SDManager::isAvailable() && SDManager::configFileExists()) {
    File configFile = SD.open("/config.json", FILE_READ);
    if (configFile) {
      const size_t maxSize = 512;
      char jsonBuffer[512];
      size_t fileSize = configFile.size();
      if (fileSize > 0 && fileSize < maxSize) {
        size_t bytesRead = configFile.readBytes(jsonBuffer, maxSize - 1);
        jsonBuffer[bytesRead] = '\0';
        configFile.close();

        // Utiliser JsonDocument pour éviter débordement de pile
        JsonDocument doc;
        if (!deserializeJson(doc, jsonBuffer)) {
          doc["timezoneId"] = timezoneId;
          configFile = SD.open("/config.json", FILE_WRITE);
          if (configFile) {
            serializeJson(doc, configFile);
            configFile.close();
            RTCManager::setTimezoneId(timezoneId);
            Serial.println("[CONFIG-SYNC] timezoneId sauvegardé dans config.json");
          }
        }
      } else {
        configFile.close();
      }
    }
  }

  // Obtenir les offsets UTC pour cette timezone
  long offsetSeconds = TimezoneManager::getOffsetSeconds(timezoneId);
  int daylightOffsetSeconds = TimezoneManager::getDaylightOffsetSeconds(timezoneId);

  Serial.printf("[CONFIG-SYNC] offsetSeconds=%ld, daylightOffsetSeconds=%d\n",
    offsetSeconds, daylightOffsetSeconds);

  // Synchroniser le RTC en UTC (IMPORTANT: RTC doit TOUJOURS être en UTC pour les signatures)
  // Le fuseau horaire est appliqué à l'app, pas au RTC
  if (RTCManager::syncWithNTP(0, 0)) {
    Serial.printf("[CONFIG-SYNC] RTC synchronisé (UTC) - Fuseau horaire: %s\n", timezoneId);
    return true;
  } else {
    Serial.println("[CONFIG-SYNC] Erreur lors de la synchronisation du RTC");
    return false;
  }

#else
  return false;
#endif
}

// Wrapper function - évite dépendance circulaire d'includes
extern "C" void retryConfigSync() {
  ModelDreamConfigSyncRoutes::retryFetchConfig();
}
