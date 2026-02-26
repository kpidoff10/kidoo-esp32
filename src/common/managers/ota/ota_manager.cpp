/**
 * OTA Manager - Mise à jour firmware par parts (HTTP).
 * Télécharge les parts via l'API /api/firmware/download, écrit via Update, puis redémarre.
 *
 * L'API renvoie désormais uniquement des URLs vers /api/firmware/serve (même origine que l'API),
 * afin d'éviter les handshakes TLS incertains vers Cloudflare R2 directement depuis l'ESP32.
 */

#include "ota_manager.h"
#include "models/model_config.h"
#include "common/config/core_config.h"
#include "common/managers/log/log_manager.h"
#include "common/managers/pubnub/pubnub_manager.h"
#ifdef HAS_BLE
#include "common/managers/ble/ble_manager.h"
#endif
#ifdef HAS_LED
#include "common/managers/led/led_manager.h"
#endif
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <Preferences.h>
#include <SD.h>
#include "common/managers/sd/sd_manager.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_heap_caps.h>
#include "certificats/ota-cert.h"

static const unsigned long OTA_NO_PROGRESS_TIMEOUT_MS = 20000;
static const size_t OTA_LOG_INTERVAL_BYTES = 32768;

/** Log détaillé de l'état mémoire (heap libre, plus gros bloc) pour diagnostiquer fragmentation */
static void logHeap(const char* tag) {
  if (!Serial) return;
  size_t free8 = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  size_t freeLegacy = ESP.getFreeHeap();
  LogManager::info("[OTA-HEAP] %s | free=%u KB | largest_block=%u KB | getFreeHeap=%u KB",
    tag, (unsigned)(free8 / 1024), (unsigned)(largest / 1024), (unsigned)(freeLegacy / 1024));
}

static bool parseApiBaseUrl(const char* baseUrl, String& host, uint16_t& port) {
  if (baseUrl == nullptr) {
    return false;
  }
  String url(baseUrl);
  url.trim();
  if (url.length() == 0) {
    return false;
  }

  int schemeIdx = url.indexOf("://");
  String scheme = "http";
  int hostStart = 0;
  if (schemeIdx >= 0) {
    scheme = url.substring(0, schemeIdx);
    hostStart = schemeIdx + 3;
  }

  int pathIdx = url.indexOf('/', hostStart);
  String authority = (pathIdx >= 0) ? url.substring(hostStart, pathIdx) : url.substring(hostStart);
  authority.trim();
  if (authority.length() == 0) {
    return false;
  }

  int colonIdx = authority.indexOf(':');
  if (colonIdx >= 0) {
    host = authority.substring(0, colonIdx);
    String portStr = authority.substring(colonIdx + 1);
    portStr.trim();
    port = (uint16_t)portStr.toInt();
    if (port == 0) {
      return false;
    }
  } else {
    host = authority;
    port = scheme.equalsIgnoreCase("https") ? 443 : 80;
  }

  host.trim();
  return host.length() > 0 && port > 0;
}

static const size_t OTA_CHUNK_SIZE = 2048;  // 2 KB, C3-friendly
static const int HTTP_TIMEOUT_MS = 15000;
static const int TLS_HANDSHAKE_TIMEOUT_MS = 15000;
static const size_t OTA_TASK_STACK_SIZE = 12288;

/** Configure le client TLS pour l'API */
static void configureSecureClient(WiFiClientSecure& client) {
  client.setCACert(OTA_CERT_PEM);
  client.setTimeout(TLS_HANDSHAKE_TIMEOUT_MS);
}

/** Configure le client TLS pour le téléchargement part (C3-friendly) */
static void configurePartClient(WiFiClientSecure& client) {
  client.setCACert(OTA_CERT_PEM);
  client.setTimeout(HTTP_TIMEOUT_MS);
}

static bool testConnection(WiFiClientSecure& client, const String& host, uint16_t port) {
  LogManager::info("[OTA] TLS connect test -> %s:%d", host.c_str(), port);
  if (!client.connect(host.c_str(), port)) {
    char errBuf[64];
    int errCode = client.lastError(errBuf, sizeof(errBuf));
    LogManager::error("[OTA] TLS connect FAILED, error=%d", errCode);
    LogManager::error("[OTA] TLS error detail: %s", errBuf);
    return false;
  }
  LogManager::info("[OTA] TLS connected, remote IP: %s", client.remoteIP().toString().c_str());
  client.stop();
  LogManager::info("[OTA] TLS connect OK");
  return true;
}


static char s_otaVersion[32];
static bool s_otaFreedResources = false;
static bool s_otaFreedLed = false;
static bool s_otaFreedBle = false;

/** Mode OTA : arrêt complet PubNub (task+queue), LED (task+queue+strip), BLE (deinit+mem_release) */
static void enterOtaMode() {
  s_otaFreedResources = true;  // Bloque reconnexions auto (main loop, wifi_manager)

  logHeap("avant enterOtaMode");

#ifdef HAS_PUBNUB
  if (PubNubManager::isInitialized()) {
    LogManager::info("[OTA] PubNub shutdownForOta...");
    PubNubManager::shutdownForOta();
    vTaskDelay(pdMS_TO_TICKS(100));
    logHeap("apres PubNub shutdown");
  }
#endif

#ifdef HAS_LED
  if (LEDManager::isInitialized()) {
    s_otaFreedLed = true;
    LogManager::info("[OTA] LED stop...");
    LEDManager::stop();
    vTaskDelay(pdMS_TO_TICKS(100));
    logHeap("apres LED stop");
  }
#endif

#ifdef HAS_BLE
  if (BLEManager::isInitialized()) {
    s_otaFreedBle = true;
    LogManager::info("[OTA] BLE shutdownForOta...");
    BLEManager::shutdownForOta();
    vTaskDelay(pdMS_TO_TICKS(100));
    logHeap("apres BLE shutdown");
  }
#endif

  vTaskDelay(pdMS_TO_TICKS(200));
  logHeap("enterOtaMode termine");
}

static void otaTask(void* pvParameters) {
  const char* version = (const char*)pvParameters;
  (void)OTAManager::performUpdate(version);
  vTaskDelete(NULL);
}

bool OTAManager::isOtaInProgress() {
  return s_otaFreedResources;
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

/** Publie directement (avant enterOtaMode, PubNub actif). */
static void publishFirmwareUpdateFailed(const char* version, const char* error) {
  char msg[256];
  snprintf(msg, sizeof(msg),
    "{\"type\":\"firmware-update-failed\",\"version\":\"%s\",\"error\":\"%s\"}",
    version, error);
  PubNubManager::publish(msg);
}

/** Stocke l'erreur OTA en NVS puis redémarre. La publication se fera au prochain boot. */
static void storeOtaErrorAndRestart(const char* version, const char* error) {
  Preferences prefs;
  if (prefs.begin("ota", false)) {
    prefs.putString("last_error", error ? error : "");
    prefs.putString("last_version", version ? version : "");
    prefs.end();
    LogManager::info("[OTA] Erreur stockee en NVS, reboot...");
  }
  ESP.restart();
}

static const char* OTA_DONE_FILE = "/ota_done.txt";

/** Publie le statut OTA stocké en NVS ou SD (succès ou échec). L'app attend firmware-update-done ou firmware-update-failed.
 *  Peut être rappelé plusieurs fois. SD utilisée comme fallback si NVS échoue (persistance après OTA). */
void OTAManager::publishLastOtaErrorIfAny() {
#ifdef HAS_PUBNUB
  String successVer;
  String err;
  String ver;

  Preferences prefs;
  if (prefs.begin("ota", true)) {
    successVer = prefs.getString("last_success_version", "");
    err = prefs.getString("last_error", "");
    ver = prefs.getString("last_version", "");
    prefs.end();
  }

#ifdef HAS_SD
  if (successVer.length() == 0 && SDManager::isAvailable() && SD.exists(OTA_DONE_FILE)) {
    File f = SD.open(OTA_DONE_FILE, FILE_READ);
    if (f) {
      successVer = f.readStringUntil('\n');
      successVer.trim();
      f.close();
      LogManager::info("[OTA] SD ota_done.txt version=%s", successVer.c_str());
    }
  }
#endif

  if (successVer.length() > 0) {
    LogManager::info("[OTA] last_success_version=%s", successVer.c_str());
    char msg[128];
    snprintf(msg, sizeof(msg), "{\"type\":\"firmware-update-done\",\"version\":\"%s\"}", successVer.c_str());
    if (PubNubManager::publish(msg)) {
      if (prefs.begin("ota", false)) {
        prefs.remove("last_success_version");
        prefs.end();
      }
#ifdef HAS_SD
      if (SDManager::isAvailable() && SD.exists(OTA_DONE_FILE)) {
        SD.remove(OTA_DONE_FILE);
      }
#endif
      LogManager::info("[OTA] Succes OTA publie: %s", successVer.c_str());
    } else {
      LogManager::info("[OTA] Publication OTA succes impossible (PubNub hors ligne) -> retry plus tard");
    }
  }

  if (err.length() > 0) {
    char msg[256];
    snprintf(msg, sizeof(msg),
      "{\"type\":\"firmware-update-failed\",\"version\":\"%s\",\"error\":\"%s\"}",
      ver.c_str(), err.c_str());
    if (PubNubManager::publish(msg)) {
      if (prefs.begin("ota", false)) {
        prefs.remove("last_error");
        prefs.remove("last_version");
        prefs.end();
      }
      LogManager::info("[OTA] Erreur precedente publiee: %s", err.c_str());
    } else {
      LogManager::info("[OTA] Publication OTA erreur impossible (PubNub hors ligne) -> retry plus tard");
    }
  }
#endif
}

bool OTAManager::performUpdate(const char* version) {
  if (version == nullptr || strlen(version) == 0) {
    publishFirmwareUpdateFailed("", "version manquante");
    return false;
  }

  if (WiFi.status() != WL_CONNECTED) {
    if (Serial) Serial.println("[OTA] WiFi déconnecté, annulation");
    publishFirmwareUpdateFailed(version, "wifi offline");
    return false;
  }

  String apiHost;
  uint16_t apiPort = 0;
  if (!parseApiBaseUrl(API_BASE_URL, apiHost, apiPort)) {
    LogManager::error("[OTA] Impossible de parser API_BASE_URL");
    publishFirmwareUpdateFailed(version, "url invalide");
    return false;
  }

  LogManager::info("[OTA] Base URL host: %s:%d", apiHost.c_str(), apiPort);

#ifdef HAS_LED
  if (LEDManager::isInitialized()) {
    LEDManager::wakeUp();
    LEDManager::preventSleep();
    LEDManager::setEffect(LED_EFFECT_RAINBOW);
  }
#endif

  String downloadUrl = String(API_BASE_URL) + "/api/firmware/download?model=" + KIDOO_MODEL_ID + "&version=" + String(version);
  LogManager::info("[OTA] GET %s", downloadUrl.c_str());

  enterOtaMode();

  logHeap("avant TLS connect");
  WiFiClientSecure tlsProbe;
  configureSecureClient(tlsProbe);
  if (!testConnection(tlsProbe, apiHost, apiPort)) {
    logHeap("TLS connect ECHEC");
    storeOtaErrorAndRestart(version, "tls connect");
    return false;
  }
  logHeap("TLS connect OK");
  WiFiClientSecure downloadClient;
  configureSecureClient(downloadClient);

  HTTPClient http;
  http.begin(downloadClient, downloadUrl);
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  int code = http.GET();
  String payload = http.getString();
  http.end();
  downloadClient.stop();
  vTaskDelay(pdMS_TO_TICKS(300));  // Laisser la connexion se fermer avant la suivante

  const size_t maxLog = 600;
  LogManager::info("[OTA] API reponse code=%d", code);
  if (payload.length() > maxLog) {
    LogManager::info("[OTA] API body (tronque %u/%u):", (unsigned)maxLog, (unsigned)payload.length());
    String truncated = payload.substring(0, maxLog) + "...";
    LogManager::info("%s", truncated.c_str());
  } else {
    LogManager::info("[OTA] API body: %s", payload.c_str());
  }

  if (code != HTTP_CODE_OK) {
    char err[64];
    snprintf(err, sizeof(err), "API download %d", code);
    if (Serial) {
      Serial.print("[OTA] API error: ");
      Serial.println(HTTPClient::errorToString(code));
    }
    storeOtaErrorAndRestart(version, err);
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    LogManager::error("[OTA] JSON parse error: %s", err.c_str());
    storeOtaErrorAndRestart(version, "JSON invalide");
    return false;
  }

  JsonObject data = doc["data"].as<JsonObject>();
  if (data.isNull()) {
    LogManager::error("[OTA] API sans champ 'data'");
    storeOtaErrorAndRestart(version, "reponse API sans data");
    return false;
  }

  int partCount = data["partCount"] | 1;
  size_t totalSize = (size_t)(data["totalSize"].as<long long>());
  if (totalSize == 0) {
    storeOtaErrorAndRestart(version, "totalSize manquant");
    return false;
  }

  if (!Update.begin(totalSize)) {
    char errMsg[64];
    snprintf(errMsg, sizeof(errMsg), "Update.begin %u", Update.getError());
    if (Serial) Update.printError(Serial);
    storeOtaErrorAndRestart(version, errMsg);
    return false;
  }

  bool useUrls = partCount > 1 && data["urls"].is<JsonArray>();
  const char* singleUrl = data["url"].as<const char*>();

  for (int i = 0; i < partCount; i++) {
    String partUrl;
    if (useUrls) {
      partUrl = data["urls"][i].as<String>();
    } else if (i == 0 && singleUrl != nullptr) {
      partUrl = String(singleUrl);
    } else {
      publishFirmwareUpdateFailed(version, "url(s) manquante(s)");
      Update.abort();
      return false;
    }

    LogManager::info("[OTA] Part %d url: %s", i, partUrl.c_str());

    String partHost;
    uint16_t partPort = 0;
    (void)parseApiBaseUrl(partUrl.c_str(), partHost, partPort);
    if (Serial && partHost.length() > 0) {
      Serial.print("[OTA] Part host=");
      Serial.print(partHost);
      Serial.print(" port=");
      Serial.println(partPort);
      IPAddress partIp;
      if (WiFi.hostByName(partHost.c_str(), partIp)) {
        Serial.print("[OTA] Part DNS resolved: ");
        Serial.println(partIp.toString());
      } else {
        Serial.println("[OTA] Part DNS resolution failed");
      }
      Serial.println("[OTA] HTTPS client secure (WiFiClientSecure)");
    }

    WiFiClientSecure partClient;
    configurePartClient(partClient);

    HTTPClient partHttp;
    partHttp.setTimeout(HTTP_TIMEOUT_MS);
    partHttp.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    partHttp.addHeader("Connection", "close");
    partHttp.begin(partClient, partUrl);
    int partCode = partHttp.GET();

    int cl = partHttp.getSize();
    LogManager::info("[OTA] GET part %d code=%d", i, partCode);
    LogManager::info("[OTA] Content-Length: %d", cl);
    if (partHttp.hasHeader("Transfer-Encoding")) {
      LogManager::info("[OTA] Transfer-Encoding: %s", partHttp.header("Transfer-Encoding").c_str());
    }
    if (partHttp.hasHeader("Content-Type")) {
      LogManager::info("[OTA] Content-Type: %s", partHttp.header("Content-Type").c_str());
    }
    if (partCode < 0) {
      LogManager::error("[OTA] GET part %d error: %s", i, HTTPClient::errorToString(partCode).c_str());
    }

    if (partCode != HTTP_CODE_OK) {
      char errMsg[64];
      snprintf(errMsg, sizeof(errMsg), "GET part %d: %d", i, partCode);
      LogManager::error("%s", errMsg);
      partHttp.end();
      partClient.stop();
      Update.abort();
      storeOtaErrorAndRestart(version, errMsg);
      return false;
    }

    size_t expectedBytes = (size_t)(partHttp.getSize() > 0 ? partHttp.getSize() : (partCount == 1 ? (int)totalSize : (int)(totalSize / partCount)));
    Stream* stream = partHttp.getStreamPtr();
    if (!stream) {
      partHttp.end();
      partClient.stop();
      Update.abort();
      storeOtaErrorAndRestart(version, "getStreamPtr null");
      return false;
    }

    uint8_t buf[OTA_CHUNK_SIZE];
    size_t written = 0;
    unsigned long lastProgress = millis();
    size_t lastLogged = 0;

    while (written < expectedBytes) {
      int avail = stream->available();
      if (avail <= 0) {
        if (!partClient.connected()) break;
        if (millis() - lastProgress > OTA_NO_PROGRESS_TIMEOUT_MS) {
          LogManager::info("[OTA] Timeout sans progres (20s)");
          break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
        continue;
      }

      size_t toRead = (size_t)avail;
      if (toRead > sizeof(buf)) toRead = sizeof(buf);
      if (toRead > expectedBytes - written) toRead = expectedBytes - written;

      int n = stream->readBytes(buf, toRead);
      if (n <= 0) {
        if (millis() - lastProgress > OTA_NO_PROGRESS_TIMEOUT_MS) break;
        vTaskDelay(pdMS_TO_TICKS(10));
        continue;
      }

      size_t w = Update.write(buf, (size_t)n);
      if (w != (size_t)n) {
        char errMsg[64];
        snprintf(errMsg, sizeof(errMsg), "Update.write part %d", i);
        if (Serial) Update.printError(Serial);
        partHttp.end();
        partClient.stop();
        Update.abort();
        storeOtaErrorAndRestart(version, errMsg);
        return false;
      }

      written += w;
      lastProgress = millis();

      if (written - lastLogged >= OTA_LOG_INTERVAL_BYTES) {
        LogManager::info("[OTA] Part %d downloaded: %u/%u KB", i, (unsigned)(written / 1024), (unsigned)(expectedBytes / 1024));
        lastLogged = written;
      }
    }

    partHttp.end();
    partClient.stop();

    if (written != expectedBytes) {
      char errMsg[96];
      snprintf(errMsg, sizeof(errMsg), "Part %d incomplete: %u/%u", i, (unsigned)written, (unsigned)expectedBytes);
      if (Serial) Serial.println(errMsg);
      Update.abort();
      storeOtaErrorAndRestart(version, errMsg);
      return false;
    }

    LogManager::info("[OTA] Part %d written: %u bytes", i, (unsigned)written);
  }

  if (!Update.end(true)) {
    char errMsg[64];
    snprintf(errMsg, sizeof(errMsg), "Update.end %u", Update.getError());
    if (Serial) Update.printError(Serial);
    storeOtaErrorAndRestart(version, errMsg);
    return false;
  }

  /* Succès : stocker la version en NVS et SD pour publier "firmware-update-done" au prochain boot.
   * L'app (via le serveur) attend ce message pour sortir de "Mise à jour en cours". */
  Preferences prefs;
  if (prefs.begin("ota", false)) {
    prefs.putString("last_success_version", version);
    prefs.end();
  }
#ifdef HAS_SD
  if (SDManager::isAvailable()) {
    File f = SD.open(OTA_DONE_FILE, FILE_WRITE);
    if (f) {
      f.println(version);
      f.close();
      LogManager::info("[OTA] Succes stocke sur SD");
    }
  }
#endif
  LogManager::info("[OTA] Reboot...");
  vTaskDelay(pdMS_TO_TICKS(200));
  ESP.restart();
  return true;
}
