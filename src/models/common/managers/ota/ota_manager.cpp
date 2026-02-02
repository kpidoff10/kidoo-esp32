/**
 * OTA Manager - Mise à jour firmware par parts (HTTP).
 * Télécharge les parts via l'API /api/firmware/download, écrit via Update, puis redémarre.
 *
 * Pourquoi l'URL directe R2 (https://pub-xxx.r2.dev/...) échoue souvent (-1) sur l'ESP32 :
 * - Connexion HTTPS vers un NOUVEAU domaine (R2) = nouveau handshake TLS.
 * - Le certificat Cloudflare/R2 peut ne pas être dans le bundle CA par défaut de l'ESP32,
 *   ou la chaîne de certificats / la config TLS peut faire échouer la vérification.
 * - D'où le fallback sur l'URL serve (même origine que l'API), qui réutilise un domaine
 *   déjà réussi (ex. kidoo-box.com ou ton IP LAN) et est plus fiable sur l'ESP.
 */

#include "ota_manager.h"
#include "../../../model_config.h"
#include "../../config/core_config.h"
#include "../pubnub/pubnub_manager.h"
#ifdef HAS_LED
#include "../led/led_manager.h"
#endif
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#ifndef API_BASE_URL
#define API_BASE_URL "https://kidoo-box.com"
#endif

static const size_t OTA_CHUNK_SIZE = 4096;
static const int HTTP_TIMEOUT_MS = 60000;
static const size_t OTA_TASK_STACK_SIZE = 12288;

static char s_otaVersion[32];

static void otaTask(void* pvParameters) {
  const char* version = (const char*)pvParameters;
  (void)OTAManager::performUpdate(version);
  vTaskDelete(NULL);
}

bool OTAManager::startUpdateTask(const char* version) {
  if (version == nullptr || strlen(version) == 0) {
    return false;
  }
  if (strlen(version) >= sizeof(s_otaVersion)) {
    return false;
  }
  strncpy(s_otaVersion, version, sizeof(s_otaVersion) - 1);
  s_otaVersion[sizeof(s_otaVersion) - 1] = '\0';
  BaseType_t created = xTaskCreate(otaTask, "ota", OTA_TASK_STACK_SIZE, s_otaVersion, 1, NULL);
  return (created == pdPASS);
}

static void otaAllowSleep(void) {
#ifdef HAS_LED
  if (LEDManager::isInitialized()) {
    LEDManager::allowSleep();
  }
#endif
}

static void publishFirmwareUpdateFailed(const char* version, const char* error) {
  char msg[256];
  snprintf(msg, sizeof(msg),
    "{\"type\":\"firmware-update-failed\",\"version\":\"%s\",\"error\":\"%s\"}",
    version, error);
  PubNubManager::publish(msg);
}

bool OTAManager::performUpdate(const char* version) {
  if (version == nullptr || strlen(version) == 0) {
    publishFirmwareUpdateFailed("", "version manquante");
    return false;
  }

#ifdef HAS_LED
  if (LEDManager::isInitialized()) {
    LEDManager::wakeUp();
    LEDManager::preventSleep();
    LEDManager::setEffect(LED_EFFECT_RAINBOW);
  }
#endif

  String downloadUrl = String(API_BASE_URL) + "/api/firmware/download?model=" + KIDOO_MODEL_ID + "&version=" + String(version);
  if (Serial) {
    Serial.println("[OTA] GET " + downloadUrl);
  }
  HTTPClient http;
  http.begin(downloadUrl);
  http.setTimeout(HTTP_TIMEOUT_MS);

  int code = http.GET();
  String payload = http.getString();
  http.end();

  if (Serial) {
    Serial.print("[OTA] API reponse code=");
    Serial.println(code);
    const size_t maxLog = 600;
    if (payload.length() > maxLog) {
      Serial.print("[OTA] API body (tronque ");
      Serial.print(maxLog);
      Serial.print("/");
      Serial.print(payload.length());
      Serial.println("):");
      Serial.println(payload.substring(0, maxLog) + "...");
    } else {
      Serial.print("[OTA] API body: ");
      Serial.println(payload);
    }
  }

  if (code != HTTP_CODE_OK) {
    char err[64];
    snprintf(err, sizeof(err), "API download %d", code);
    publishFirmwareUpdateFailed(version, err);
    otaAllowSleep();
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    if (Serial) Serial.println("[OTA] JSON parse error: " + String(err.c_str()));
    publishFirmwareUpdateFailed(version, "JSON invalide");
    otaAllowSleep();
    return false;
  }

  JsonObject data = doc["data"].as<JsonObject>();
  if (data.isNull()) {
    if (Serial) Serial.println("[OTA] API sans champ 'data'");
    publishFirmwareUpdateFailed(version, "reponse API sans data");
    otaAllowSleep();
    return false;
  }

  int partCount = data["partCount"] | 1;
  size_t totalSize = (size_t)(data["totalSize"].as<long long>());
  if (totalSize == 0) {
    publishFirmwareUpdateFailed(version, "totalSize manquant");
    otaAllowSleep();
    return false;
  }

  if (!Update.begin(totalSize)) {
    char errMsg[64];
    snprintf(errMsg, sizeof(errMsg), "Update.begin %u", Update.getError());
    publishFirmwareUpdateFailed(version, errMsg);
    otaAllowSleep();
    return false;
  }

  bool useUrls = partCount > 1 && data["urls"].is<JsonArray>();
  const char* singleUrl = data["url"].as<const char*>();
  const char* singleFallback = data["fallbackUrl"].as<const char*>();
  bool useFallbackUrls = partCount > 1 && data["fallbackUrls"].is<JsonArray>();

  for (int i = 0; i < partCount; i++) {
    String partUrl;
    String fallbackPartUrl;
    if (useUrls) {
      partUrl = data["urls"][i].as<String>();
      if (useFallbackUrls) {
        fallbackPartUrl = data["fallbackUrls"][i].as<String>();
      }
    } else if (i == 0 && singleUrl != nullptr) {
      partUrl = String(singleUrl);
      if (singleFallback != nullptr) {
        fallbackPartUrl = String(singleFallback);
      }
    } else {
      publishFirmwareUpdateFailed(version, "url(s) manquante(s)");
      Update.abort();
      return false;
    }

    if (Serial) {
      Serial.print("[OTA] Part ");
      Serial.print(i);
      Serial.print(" url: ");
      Serial.println(partUrl.length() > 80 ? partUrl.substring(0, 80) + "..." : partUrl);
      if (fallbackPartUrl.length() > 0) {
        Serial.print("[OTA] Part ");
        Serial.print(i);
        Serial.print(" fallback: ");
        Serial.println(fallbackPartUrl.length() > 80 ? fallbackPartUrl.substring(0, 80) + "..." : fallbackPartUrl);
      }
    }

    HTTPClient partHttp;
    HTTPClient fallbackHttp;
    HTTPClient* current = &partHttp;
    partHttp.setTimeout(HTTP_TIMEOUT_MS);
    partHttp.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    partHttp.begin(partUrl);
    int partCode = partHttp.GET();
    if (Serial) {
      Serial.print("[OTA] GET part ");
      Serial.print(i);
      Serial.print(" code=");
      Serial.println(partCode);
    }
    if (partCode != HTTP_CODE_OK && fallbackPartUrl.length() > 0) {
      partHttp.end();
      delay(500);  // Laisser le WiFi/TLS liberer apres echec HTTPS
      if (Serial) Serial.println("[OTA] Essai fallback (nouveau client)...");
      fallbackHttp.setTimeout(HTTP_TIMEOUT_MS);
      fallbackHttp.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
      fallbackHttp.begin(fallbackPartUrl);
      partCode = fallbackHttp.GET();
      if (partCode == HTTP_CODE_OK) {
        current = &fallbackHttp;
      }
      if (Serial) {
        Serial.print("[OTA] GET part ");
        Serial.print(i);
        Serial.print(" fallback code=");
        Serial.println(partCode);
      }
    }
    if (partCode != HTTP_CODE_OK) {
      char errMsg[64];
      snprintf(errMsg, sizeof(errMsg), "GET part %d: %d", i, partCode);
      if (Serial) {
        Serial.print("[OTA] Echec: ");
        Serial.println(errMsg);
      }
      publishFirmwareUpdateFailed(version, errMsg);
      current->end();
      Update.abort();
      otaAllowSleep();
      return false;
    }

    int len = current->getSize();
    size_t written = 0;
    uint8_t buf[OTA_CHUNK_SIZE];
    while (current->connected() && (len < 0 || written < (size_t)len)) {
      size_t toRead = OTA_CHUNK_SIZE;
      if (len > 0 && (size_t)len - written < OTA_CHUNK_SIZE) {
        toRead = (size_t)len - written;
      }
      int r = current->getStream().readBytes(buf, toRead);
      if (r <= 0) break;
      if (Update.write(buf, (size_t)r) != (size_t)r) {
        char errMsg[64];
        snprintf(errMsg, sizeof(errMsg), "Update.write part %d", i);
        publishFirmwareUpdateFailed(version, errMsg);
        current->end();
        Update.abort();
        otaAllowSleep();
        return false;
      }
      written += (size_t)r;
    }
    current->end();
  }

  if (!Update.end(true)) {
    char errMsg[64];
    snprintf(errMsg, sizeof(errMsg), "Update.end %u", Update.getError());
    publishFirmwareUpdateFailed(version, errMsg);
    otaAllowSleep();
    return false;
  }

  ESP.restart();
  return true;
}
