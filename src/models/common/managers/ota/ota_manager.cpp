/**
 * OTA Manager - Mise à jour firmware par parts (HTTP).
 * Télécharge les parts via l'API /api/firmware/download, écrit via Update, puis redémarre.
 *
 * L'API renvoie désormais uniquement des URLs vers /api/firmware/serve (même origine que l'API),
 * afin d'éviter les handshakes TLS incertains vers Cloudflare R2 directement depuis l'ESP32.
 */

#include "ota_manager.h"
#include "../../../model_config.h"
#include "../../config/core_config.h"
#include "../pubnub/pubnub_manager.h"
#ifdef HAS_BLE
#include "../ble/ble_manager.h"
#endif
#ifdef HAS_LED
#include "../led/led_manager.h"
#endif
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <Preferences.h>
#include <SD.h>
#include "../sd/sd_manager.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_heap_caps.h>
#include "../../../../../certificats/ota-cert.h"

static const unsigned long OTA_NO_PROGRESS_TIMEOUT_MS = 20000;
static const size_t OTA_LOG_INTERVAL_BYTES = 32768;

/** Log détaillé de l'état mémoire (heap libre, plus gros bloc) pour diagnostiquer fragmentation */
static void logHeap(const char* tag) {
  if (!Serial) return;
  size_t free8 = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  size_t freeLegacy = ESP.getFreeHeap();
  Serial.print("[OTA-HEAP] ");
  Serial.print(tag);
  Serial.print(" | free=");
  Serial.print(free8 / 1024);
  Serial.print(" KB | largest_block=");
  Serial.print(largest / 1024);
  Serial.print(" KB | getFreeHeap=");
  Serial.print(freeLegacy / 1024);
  Serial.println(" KB");
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
  if (Serial) {
    Serial.print("[OTA] TLS connect test -> ");
    Serial.print(host);
    Serial.print(":");
    Serial.println(port);
  }
  if (!client.connect(host.c_str(), port)) {
    if (Serial) {
      char errBuf[64];
      int errCode = client.lastError(errBuf, sizeof(errBuf));
      Serial.print("[OTA] TLS connect FAILED, error=");
      Serial.println(errCode);
      Serial.print("[OTA] TLS error detail: ");
      Serial.println(errBuf);
    }
    return false;
  }
  if (Serial) {
    Serial.print("[OTA] TLS connected, remote IP: ");
    Serial.println(client.remoteIP());
  }
  client.stop();
  if (Serial) Serial.println("[OTA] TLS connect OK");
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
    if (Serial) Serial.println("[OTA] PubNub shutdownForOta...");
    PubNubManager::shutdownForOta();
    vTaskDelay(pdMS_TO_TICKS(100));
    logHeap("apres PubNub shutdown");
  }
#endif

#ifdef HAS_LED
  if (LEDManager::isInitialized()) {
    s_otaFreedLed = true;
    if (Serial) Serial.println("[OTA] LED stop...");
    LEDManager::stop();
    vTaskDelay(pdMS_TO_TICKS(100));
    logHeap("apres LED stop");
  }
#endif

#ifdef HAS_BLE
  if (BLEManager::isInitialized()) {
    s_otaFreedBle = true;
    if (Serial) Serial.println("[OTA] BLE shutdownForOta...");
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
    if (Serial) Serial.println("[OTA] Erreur stockee en NVS, reboot...");
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
      if (Serial) {
        Serial.print("[OTA] SD ota_done.txt version=");
        Serial.println(successVer);
      }
    }
  }
#endif

  if (successVer.length() > 0) {
    if (Serial) {
      Serial.print("[OTA] last_success_version=");
      Serial.println(successVer);
    }
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
      if (Serial) {
        Serial.print("[OTA] Succes OTA publie: ");
        Serial.println(successVer);
      }
    } else if (Serial) {
      Serial.println("[OTA] Publication OTA succes impossible (PubNub hors ligne) -> retry plus tard");
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
      if (Serial) {
        Serial.print("[OTA] Erreur precedente publiee: ");
        Serial.println(err);
      }
    } else if (Serial) {
      Serial.println("[OTA] Publication OTA erreur impossible (PubNub hors ligne) -> retry plus tard");
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
    if (Serial) Serial.println("[OTA] Impossible de parser API_BASE_URL");
    publishFirmwareUpdateFailed(version, "url invalide");
    return false;
  }

  if (Serial) {
    Serial.print("[OTA] Base URL host: ");
    Serial.print(apiHost);
    Serial.print(":");
    Serial.println(apiPort);
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
    if (Serial) Serial.println("[OTA] JSON parse error: " + String(err.c_str()));
    storeOtaErrorAndRestart(version, "JSON invalide");
    return false;
  }

  JsonObject data = doc["data"].as<JsonObject>();
  if (data.isNull()) {
    if (Serial) Serial.println("[OTA] API sans champ 'data'");
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

    if (Serial) {
      Serial.print("[OTA] Part ");
      Serial.print(i);
      Serial.print(" url: ");
      Serial.println(partUrl);
    }

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

    if (Serial) {
      Serial.print("[OTA] GET part ");
      Serial.print(i);
      Serial.print(" code=");
      Serial.println(partCode);
      int cl = partHttp.getSize();
      Serial.print("[OTA] Content-Length: ");
      Serial.println(cl);
      if (partHttp.hasHeader("Transfer-Encoding")) {
        Serial.print("[OTA] Transfer-Encoding: ");
        Serial.println(partHttp.header("Transfer-Encoding"));
      }
      if (partHttp.hasHeader("Content-Type")) {
        Serial.print("[OTA] Content-Type: ");
        Serial.println(partHttp.header("Content-Type"));
      }
      if (partCode < 0) {
        Serial.print("[OTA] GET part ");
        Serial.print(i);
        Serial.print(" error: ");
        Serial.println(HTTPClient::errorToString(partCode));
      }
    }

    if (partCode != HTTP_CODE_OK) {
      char errMsg[64];
      snprintf(errMsg, sizeof(errMsg), "GET part %d: %d", i, partCode);
      if (Serial) Serial.println(errMsg);
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
          if (Serial) Serial.println("[OTA] Timeout sans progres (20s)");
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

      if (Serial && written - lastLogged >= OTA_LOG_INTERVAL_BYTES) {
        Serial.print("[OTA] Part ");
        Serial.print(i);
        Serial.print(" downloaded: ");
        Serial.print(written / 1024);
        Serial.print("/");
        Serial.print(expectedBytes / 1024);
        Serial.println(" KB");
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

    if (Serial) {
      Serial.print("[OTA] Part ");
      Serial.print(i);
      Serial.print(" written: ");
      Serial.print(written);
      Serial.println(" bytes");
    }
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
      if (Serial) Serial.println("[OTA] Succes stocke sur SD");
    }
  }
#endif
  if (Serial) Serial.println("[OTA] Reboot...");
  vTaskDelay(pdMS_TO_TICKS(200));
  ESP.restart();
  return true;
}
