/**
 * OTA Manager - Mise à jour firmware Over-The-Air
 * Télécharge le .bin depuis l'API, écrit en partition OTA, LED arc-en-ciel pendant l'update,
 * publie "firmware-update-done" ou "firmware-update-failed" via PubNub, puis redémarre.
 */

#include "ota_manager.h"
#include "../../config/default_config.h"
#include "../../config/core_config.h"

#ifdef HAS_WIFI

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <esp_system.h>
#include <esp_ota_ops.h>
#include <esp_err.h>
#include "esp_https_ota.h"
#include "esp_http_client.h"
// CHANGE: Include pour certificate bundle TLS
#include "esp_crt_bundle.h"

#include "../led/led_manager.h"
#include "../wifi/wifi_manager.h"
#include "../pubnub/pubnub_manager.h"
#include "../../utils/mac_utils.h"
#ifdef HAS_BLE
#include "../ble/ble_manager.h"
#endif
#ifdef HAS_SD
#include "../sd/sd_manager.h"
#include <SD.h>
#endif

// CHANGE: Flag de build pour autoriser TLS insecure (par défaut 0 = sécurisé)
// Définir -DOTA_ALLOW_INSECURE_TLS=1 dans platformio.ini pour activer (non recommandé en prod)
#ifndef OTA_ALLOW_INSECURE_TLS
#define OTA_ALLOW_INSECURE_TLS 0
#endif

// CHANGE: Seuils de heap pour choisir la stratégie OTA
// Si heap < HEAP_MIN_FOR_HTTPS_OTA : ne jamais utiliser esp_https_ota, basculer sur fallback HTTP + SD
// Si heap >= HEAP_MIN_FOR_SD_MODE : activer mode SD (téléchargement sur SD puis flash depuis SD)
#define HEAP_MIN_FOR_HTTPS_OTA 40000
#define HEAP_MIN_FOR_SD_MODE   20000

// Chunks plus petits = écriture flash plus courte = moins de blocage flash (lecture LED possible entre chaque chunk)
#define OTA_BUFFER_SIZE 512
// Écriture SD par blocs de 512 octets (taille secteur) pour plus de stabilité sur ESP32-C3
#define OTA_SD_WRITE_CHUNK 512
#define OTA_SD_WRITE_RETRIES 3
// Délai (ms) entre chunks pendant flash depuis SD : laisser la tâche LED mettre à jour l'arc-en-ciel sans saccades
#define OTA_SD_FLASH_YIELD_MS 10
// CHANGE: Stack OTA augmentée à 16 KB pour réduire les échecs TLS dus au heap/stack
// Stack OTA : 16 KB pour HTTPClient + stream + SD/Update + TLS (évite abort/overflow sur heap faible)
#define OTA_TASK_STACK_SIZE 16384
// CORE_OTA et PRIORITY_OTA dans core_config.h (dual-core = OTA sur Core 1 pour ne pas bloquer les LEDs)
#define OTA_WRITE_YIELD_MS 10
#define OTA_STALL_TIMEOUT_MS 120000  // 2 min sans progression -> abandon + respiration rouge

// Paramètres passés à la tâche OTA (copie des chaînes)
struct OtaTaskParams {
  char downloadUrl[384];   // URL /api/firmware/download?model=...&version=...
  char binUrl[1024];       // URL directe R2 (publique ou presignée) ou fallback /api/firmware/serve
  char fallbackUrl[512];   // URL /api/firmware/serve pour retry si GET sur binUrl échoue (ex. -1 TLS)
  char version[32];
  char model[16];
};

// Publie la progression OTA via PubNub pour l'app (phase: "download" ou "flash", percent: 0-100)
static void publishOtaProgress(const char* version, const char* phase, int percent) {
  if (version == nullptr || phase == nullptr || !PubNubManager::isInitialized()) return;
  char msg[128];
  snprintf(msg, sizeof(msg), "{\"type\":\"firmware-update-progress\",\"phase\":\"%s\",\"percent\":%d,\"version\":\"%s\"}", phase, percent, version);
  PubNubManager::publish(msg);
}

// En cas de timeout (blocage téléchargement/écriture) : publie failed, respiration rouge 5 s, retour mode normal
static void otaAbortTimeout(const char* version) {
  if (version == nullptr) return;
  Serial.println("[OTA] Timeout (blocage) - abandon");
  char errMsg[128];
  snprintf(errMsg, sizeof(errMsg), "{\"type\":\"firmware-update-failed\",\"version\":\"%s\",\"error\":\"timeout\"}", version);
  if (PubNubManager::isInitialized()) PubNubManager::publish(errMsg);
#ifdef HAS_LED
  if (LEDManager::isInitialized()) {
    LEDManager::setColor(255, 0, 0);
    LEDManager::setEffect(LED_EFFECT_PULSE);
    vTaskDelay(pdMS_TO_TICKS(5000));
    LEDManager::setEffect(LED_EFFECT_NONE);
    LEDManager::setColor(255, 107, 107);
  }
#endif
}

static void otaTaskFunction(void* param) {
  OtaTaskParams* params = (OtaTaskParams*)param;
  if (params == nullptr) {
    vTaskDelete(nullptr);
    return;
  }

  const char* version = params->version;
  const char* model = params->model;

  Serial.println("[OTA] Démarrage de la mise à jour...");
  Serial.print("[OTA] Version cible: ");
  Serial.println(version);

#ifdef HAS_LED
  if (LEDManager::isInitialized()) {
    LEDManager::setEffect(LED_EFFECT_RAINBOW);
    Serial.println("[OTA] Effet arc-en-ciel activé pendant la mise à jour");
  }
#endif

  // 1) Récupérer l'URL du binaire depuis l'API
  HTTPClient http;
  // CHANGE: Activer le suivi de redirections pour Cloudflare/R2
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setRedirectLimit(5);
  http.begin(params->downloadUrl);
  http.setConnectTimeout(5000);
  http.setTimeout(10000);

  int apiCode = http.GET();
  
  // CHANGE: Logging détaillé après http.GET() - code HTTP, erreur si < 0, redirection, taille, headers
  Serial.print("[OTA] API download URL - Code HTTP: ");
  Serial.println(apiCode);
  if (apiCode < 0) {
    Serial.print("[OTA] Erreur HTTP: ");
    Serial.println(http.errorToString(apiCode));
  }
  String location = http.getLocation();
  if (location.length() > 0) {
    Serial.print("[OTA] Redirection vers: ");
    Serial.println(location);
  }
  int contentLength = http.getSize();
  if (contentLength > 0) {
    Serial.print("[OTA] Content-Length: ");
    Serial.println(contentLength);
  }
  String contentType = http.header("Content-Type");
  if (contentType.length() > 0) {
    Serial.print("[OTA] Content-Type: ");
    Serial.println(contentType);
  }

  if (apiCode != HTTP_CODE_OK) {
    Serial.print("[OTA] Erreur API download URL: ");
    Serial.println(apiCode);
    http.end();
    vTaskDelay(pdMS_TO_TICKS(50));
    char errMsg[128];
    snprintf(errMsg, sizeof(errMsg), "{\"type\":\"firmware-update-failed\",\"version\":\"%s\",\"error\":\"api-%d\"}", version, apiCode);
    if (PubNubManager::isInitialized()) {
      PubNubManager::publish(errMsg);
    }
#ifdef HAS_LED
    if (LEDManager::isInitialized()) {
      LEDManager::setEffect(LED_EFFECT_NONE);
    }
#endif
    delete params;
    vTaskDelete(nullptr);
    return;
  }

  String apiPayload = http.getString();
  http.end();

  // CHANGE: Utiliser JsonDocument au lieu de StaticJsonDocument (déprécié)
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, apiPayload);
  if (err || !doc["success"].as<bool>()) {
    Serial.print("[OTA] Erreur parsing réponse API: ");
    Serial.println(err.f_str());
    char errMsg[128];
    snprintf(errMsg, sizeof(errMsg), "{\"type\":\"firmware-update-failed\",\"version\":\"%s\",\"error\":\"parse\"}", version);
    if (PubNubManager::isInitialized()) {
      PubNubManager::publish(errMsg);
    }
#ifdef HAS_LED
    if (LEDManager::isInitialized()) {
      LEDManager::setEffect(LED_EFFECT_NONE);
    }
#endif
    delete params;
    vTaskDelete(nullptr);
    return;
  }

  const char* binUrl = doc["data"]["url"].as<const char*>();
  if (binUrl == nullptr || strlen(binUrl) == 0) {
    Serial.println("[OTA] URL du binaire manquante dans la réponse");
    char errMsg[128];
    snprintf(errMsg, sizeof(errMsg), "{\"type\":\"firmware-update-failed\",\"version\":\"%s\",\"error\":\"no-url\"}", version);
    if (PubNubManager::isInitialized()) {
      PubNubManager::publish(errMsg);
    }
#ifdef HAS_LED
    if (LEDManager::isInitialized()) {
      LEDManager::setEffect(LED_EFFECT_NONE);
    }
#endif
    delete params;
    vTaskDelete(nullptr);
    return;
  }

  strncpy(params->binUrl, binUrl, sizeof(params->binUrl) - 1);
  params->binUrl[sizeof(params->binUrl) - 1] = '\0';
  params->fallbackUrl[0] = '\0';
  const char* fb = doc["data"]["fallbackUrl"].as<const char*>();
  if (fb != nullptr && strlen(fb) > 0 && strlen(fb) < sizeof(params->fallbackUrl)) {
    strncpy(params->fallbackUrl, fb, sizeof(params->fallbackUrl) - 1);
    params->fallbackUrl[sizeof(params->fallbackUrl) - 1] = '\0';
  }

  Serial.print("[OTA] Téléchargement du binaire depuis: ");
  Serial.println(params->binUrl);

  // 2) Téléchargement + écriture flash via esp_https_ota (HTTPS) ou HTTPClient+Update (HTTP fallback)
  esp_err_t ota_result = ESP_FAIL;
  bool binUrlHttps = (strncmp(params->binUrl, "https://", 8) == 0);

  if (binUrlHttps) {
    // CHANGE: Vérifier que WiFi est toujours connecté avant esp_https_ota
    if (!WiFi.isConnected()) {
      Serial.println("[OTA] WiFi déconnecté avant esp_https_ota, abandon");
      ota_result = ESP_FAIL;
    } else {
      // CHANGE: Logger l'état WiFi et DNS pour diagnostic
      Serial.printf("[OTA] WiFi connecté: SSID=%s, IP=%s, RSSI=%d dBm\n", 
        WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
      IPAddress dns1 = WiFi.dnsIP(0);
      IPAddress dns2 = WiFi.dnsIP(1);
      Serial.printf("[OTA] DNS configurés: DNS1=%s, DNS2=%s\n", 
        dns1.toString().c_str(), dns2.toString().c_str());
      
      // CHANGE: Extraire et logger le hostname pour diagnostic SNI/DNS
      const char* urlStart = params->binUrl + 8; // skip "https://"
      const char* urlEnd = strchr(urlStart, '/');
      int hostnameLen = urlEnd ? (urlEnd - urlStart) : strlen(urlStart);
      char hostname[256];
      strncpy(hostname, urlStart, hostnameLen < 255 ? hostnameLen : 255);
      hostname[hostnameLen < 255 ? hostnameLen : 255] = '\0';
      Serial.print("[OTA] Hostname ciblé: ");
      Serial.println(hostname);
      
      // CHANGE: Tentative de résolution DNS initiale (sans changer le DNS par défaut)
      IPAddress resolvedIP;
      bool dnsResolved = WiFi.hostByName(hostname, resolvedIP);
      bool dnsValid = dnsResolved && (resolvedIP != IPAddress(0, 0, 0, 0));
      
      Serial.printf("[OTA] Résolution DNS initiale: %s -> %s (valide: %s)\n", 
        hostname, resolvedIP.toString().c_str(), dnsValid ? "OUI" : "NON");
      
      // CHANGE: Si résolution DNS échouée ou IP invalide (0.0.0.0), essayer avec DNS 8.8.8.8
      bool dnsChanged = false;
      if (!dnsValid) {
        Serial.println("[OTA] Résolution DNS échouée ou IP invalide (0.0.0.0), tentative avec DNS 8.8.8.8");
        WiFi.setDNS(IPAddress(8, 8, 8, 8));
        vTaskDelay(pdMS_TO_TICKS(1000)); // Délai pour laisser le DNS se mettre à jour
        dnsResolved = WiFi.hostByName(hostname, resolvedIP);
        dnsValid = dnsResolved && (resolvedIP != IPAddress(0, 0, 0, 0));
        dnsChanged = true;
        Serial.printf("[OTA] Résolution DNS avec 8.8.8.8: %s -> %s (valide: %s)\n", 
          hostname, resolvedIP.toString().c_str(), dnsValid ? "OUI" : "NON");
      }
      
      // CHANGE: Ne jamais appeler esp_https_ota si DNS invalide (0.0.0.0)
      if (!dnsValid) {
        Serial.println("[OTA] ERREUR: Résolution DNS invalide (0.0.0.0), basculement immédiat sur fallback HTTP");
        ota_result = ESP_FAIL;
      } else {
        // CHANGE: Vérifier le heap libre avant esp_https_ota (minimum HEAP_MIN_FOR_HTTPS_OTA bytes requis)
        unsigned int heapBefore = ESP.getFreeHeap();
        size_t heapMinEver = esp_get_minimum_free_heap_size();
        Serial.printf("[OTA] Heap libre avant esp_https_ota: %u bytes\n", heapBefore);
        Serial.printf("[OTA] Heap minimum ever: %u bytes\n", (unsigned)heapMinEver);
        Serial.printf("[OTA] Stratégie OTA: heap=%u, seuil HTTPS=%d, seuil SD=%d\n", 
          heapBefore, HEAP_MIN_FOR_HTTPS_OTA, HEAP_MIN_FOR_SD_MODE);
        
        // CHANGE: Si heap insuffisant pour HTTPS OTA, ne jamais tenter esp_https_ota
        if (heapBefore < HEAP_MIN_FOR_HTTPS_OTA) {
          Serial.printf("[OTA] ERREUR: Heap insuffisant pour OTA HTTPS (%u < %d), basculement sur fallback HTTP\n", 
            heapBefore, HEAP_MIN_FOR_HTTPS_OTA);
          ota_result = ESP_FAIL;
        } else {
          // CHANGE: Libération de RAM - suspendre PubNubManager et BLE pendant OTA HTTPS
          bool pubnubWasConnected = false;
          bool bleWasAdvertising = false;
          
          if (PubNubManager::isInitialized() && PubNubManager::isConnected()) {
            Serial.println("[OTA] Suspension de PubNubManager pour libérer RAM");
            pubnubWasConnected = true;
            PubNubManager::disconnect();
            vTaskDelay(pdMS_TO_TICKS(200)); // Délai pour laisser le thread se terminer
          }
          
#ifdef HAS_BLE
          if (BLEManager::isInitialized() && BLEManager::isAvailable()) {
            Serial.println("[OTA] Arrêt de l'advertising BLE pour libérer RAM");
            bleWasAdvertising = true;
            BLEManager::stopAdvertising();
            vTaskDelay(pdMS_TO_TICKS(100));
          }
#endif
          
          unsigned int heapAfterSuspend = ESP.getFreeHeap();
          Serial.printf("[OTA] Heap libre après suspension PubNub/BLE: %u bytes (delta: %d)\n", 
            heapAfterSuspend, (int)(heapAfterSuspend - heapBefore));
          
          // CHANGE: TLS sécurisé avec certificate bundle - utiliser setInsecure() seulement si OTA_ALLOW_INSECURE_TLS == 1
          esp_http_client_config_t http_cfg = {};
          http_cfg.url = params->binUrl;
          http_cfg.timeout_ms = 60000;
          
          // CHANGE: Ne jamais faire TLS "insecure" - toujours vérifier les certificats
          // Si OTA_ALLOW_INSECURE_TLS == 1, on ne devrait jamais arriver ici (heap insuffisant)
          Serial.println("[OTA] esp_https_ota - Vérification TLS via certificate bundle");
          http_cfg.crt_bundle_attach = esp_crt_bundle_attach;
          http_cfg.skip_cert_common_name_check = false;
          
          esp_https_ota_config_t ota_cfg = {};
          ota_cfg.http_config = &http_cfg;
          
          Serial.printf("[OTA] Appel esp_https_ota - hostname: %s, IP résolue: %s\n", hostname, resolvedIP.toString().c_str());
          ota_result = esp_https_ota(&ota_cfg);
          
          // CHANGE: Logger le heap libre après esp_https_ota et le résultat détaillé
          unsigned int heapAfter = ESP.getFreeHeap();
          Serial.printf("[OTA] Heap libre après esp_https_ota: %u bytes (delta: %d)\n", heapAfter, (int)(heapAfter - heapAfterSuspend));
          Serial.printf("[OTA] esp_https_ota résultat: %s (0x%x)\n", esp_err_to_name(ota_result), (unsigned)ota_result);
          
          // CHANGE: Réactiver PubNub/BLE uniquement si l'OTA HTTPS a échoué
          if (ota_result != ESP_OK) {
            if (pubnubWasConnected) {
              Serial.println("[OTA] Réactivation de PubNubManager après échec OTA HTTPS");
              PubNubManager::connect();
            }
#ifdef HAS_BLE
            if (bleWasAdvertising) {
              Serial.println("[OTA] Réactivation de l'advertising BLE après échec OTA HTTPS");
              BLEManager::startAdvertising();
            }
#endif
          }
          
          // CHANGE: Si échec et fallback HTTPS disponible, réessayer
          if (ota_result != ESP_OK && params->fallbackUrl[0] != '\0' && strncmp(params->fallbackUrl, "https://", 8) == 0) {
            Serial.println("[OTA] Retry esp_https_ota avec fallbackUrl (serve HTTPS)");
            Serial.printf("[OTA] Heap libre avant esp_https_ota (fallback HTTPS): %u bytes\n", (unsigned)ESP.getFreeHeap());
            http_cfg.url = params->fallbackUrl;
            ota_result = esp_https_ota(&ota_cfg);
            Serial.printf("[OTA] esp_https_ota (fallback HTTPS) résultat: %s (0x%x)\n", esp_err_to_name(ota_result), (unsigned)ota_result);
          }
        }
      }
      
      // CHANGE: Restaurer les DNS originaux si on les a changés
      if (dnsChanged && dns1 != IPAddress(0, 0, 0, 0)) {
        WiFi.setDNS(dns1, dns2);
        Serial.printf("[OTA] DNS restaurés après OTA HTTPS: DNS1=%s, DNS2=%s\n", 
          dns1.toString().c_str(), dns2.toString().c_str());
      }
    } // fin du else WiFi.isConnected()
  } // fin du if binUrlHttps

  // CHANGE: Fallback HTTP (serve en http://) si esp_https_ota a échoué ou si binUrl était déjà en HTTP
  // Mode SD forcé si SD disponible et heap >= HEAP_MIN_FOR_SD_MODE
  if (ota_result != ESP_OK && params->fallbackUrl[0] != '\0' && strncmp(params->fallbackUrl, "http://", 7) == 0) {
    Serial.println("[OTA] Fallback HTTP (serve)");
    Serial.println(params->fallbackUrl);
    
    unsigned int heapBeforeFallback = ESP.getFreeHeap();
    Serial.printf("[OTA] Heap libre avant fallback HTTP: %u bytes\n", heapBeforeFallback);
    
    // CHANGE: Détecter si on doit utiliser le mode SD (SD disponible + heap suffisant)
    bool useSDMode = false;
#ifdef HAS_SD
    if (SDManager::isAvailable() && heapBeforeFallback >= HEAP_MIN_FOR_SD_MODE) {
      useSDMode = true;
      Serial.println("[OTA] Mode SD activé: téléchargement sur /ota_firmware.bin puis flash depuis SD");
    } else {
      Serial.printf("[OTA] Mode SD désactivé: SD dispo=%d, heap=%u (seuil=%d)\n", 
        SDManager::isAvailable(), heapBeforeFallback, HEAP_MIN_FOR_SD_MODE);
    }
#endif
    
    HTTPClient httpFallback;
    // CHANGE: Activer les redirections pour le fallback HTTP
    httpFallback.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    httpFallback.setRedirectLimit(5);
    httpFallback.begin(params->fallbackUrl);
    // CHANGE: Augmenter les timeouts pour robustesse (connect 20s, read 60s)
    httpFallback.setConnectTimeout(20000);
    httpFallback.setTimeout(60000);
    
    int code = httpFallback.GET();
    
    // CHANGE: Logging détaillé après GET()
    Serial.print("[OTA] Fallback HTTP - Code: ");
    Serial.println(code);
    if (code < 0) {
      Serial.print("[OTA] Erreur HTTP: ");
      Serial.println(httpFallback.errorToString(code));
    }
    String location = httpFallback.getLocation();
    if (location.length() > 0) {
      Serial.print("[OTA] Redirection vers: ");
      Serial.println(location);
    }
    
    if (code == HTTP_CODE_OK) {
      int contentLength = httpFallback.getSize();
      Serial.printf("[OTA] Fallback HTTP - Content-Length: %d bytes\n", contentLength);
      
      // CHANGE: Toujours vérifier contentLength > 0
      if (contentLength <= 0) {
        Serial.println("[OTA] ERREUR: Content-Length invalide (<= 0), abandon");
        httpFallback.end();
        char errMsg[128];
        snprintf(errMsg, sizeof(errMsg), "{\"type\":\"firmware-update-failed\",\"version\":\"%s\",\"error\":\"invalid-size\"}", version);
        if (PubNubManager::isInitialized()) {
          PubNubManager::publish(errMsg);
        }
      } else {
#ifdef HAS_SD
        if (useSDMode) {
          // CHANGE: Mode SD - télécharger sur /ota_firmware.bin puis flasher depuis SD
          const char* sdFilePath = "/ota_firmware.bin";
          File sdFile = SD.open(sdFilePath, FILE_WRITE);
          if (!sdFile) {
            Serial.printf("[OTA] ERREUR: Impossible d'ouvrir %s en écriture\n", sdFilePath);
            httpFallback.end();
            ota_result = ESP_FAIL;
          } else {
            Serial.printf("[OTA] Téléchargement sur SD: %s (%d bytes)\n", sdFilePath, contentLength);
            WiFiClient* stream = httpFallback.getStreamPtr();
            uint8_t buf[OTA_BUFFER_SIZE];
            size_t written = 0;
            bool ok = true;
            unsigned long lastProgressMs = millis();
            
            while (httpFallback.connected() && written < (size_t)contentLength && ok) {
              // CHANGE: Stall watchdog avec logging détaillé
              unsigned long elapsed = millis() - lastProgressMs;
              if (elapsed > 30000) { // 30s sans progression
                int avail = stream->available();
                Serial.printf("[OTA] Stall détecté (SD download) - written: %u/%d, avail: %d, elapsed: %lu ms\n", 
                  (unsigned)written, contentLength, avail, elapsed);
              }
              
              size_t toRead = ((size_t)contentLength - written) > OTA_BUFFER_SIZE ? OTA_BUFFER_SIZE : ((size_t)contentLength - written);
              int n = stream->readBytes(buf, toRead);
              if (n <= 0) { 
                vTaskDelay(pdMS_TO_TICKS(10)); 
                continue; 
              }
              
              // CHANGE: Écrire sur SD avec retries
              int retries = OTA_SD_WRITE_RETRIES;
              size_t sdWritten = 0;
              while (retries > 0 && sdWritten < (size_t)n) {
                sdWritten = sdFile.write(buf + sdWritten, n - sdWritten);
                if (sdWritten < (size_t)n) {
                  retries--;
                  vTaskDelay(pdMS_TO_TICKS(10));
                }
              }
              
              if (sdWritten != (size_t)n) {
                Serial.printf("[OTA] ERREUR: Écriture SD incomplète: %u/%d bytes\n", (unsigned)sdWritten, n);
                ok = false;
                break;
              }
              
              written += (size_t)n;
              lastProgressMs = millis();
              
              // CHANGE: Publier progression
              int pct = (written * 100) / contentLength;
              if (written % (contentLength / 10) < OTA_BUFFER_SIZE || written == (size_t)contentLength) {
                publishOtaProgress(version, "download", pct);
                Serial.printf("[OTA] SD download: %u/%d bytes (%d%%)\n", (unsigned)written, contentLength, pct);
              }
              
              taskYIELD();
            }
            
            sdFile.close();
            httpFallback.end();
            
            // CHANGE: Vérifier que la taille téléchargée == Content-Length
            if (!ok || written != (size_t)contentLength) {
              Serial.printf("[OTA] ERREUR: Téléchargement SD incomplet: %u/%d bytes\n", (unsigned)written, contentLength);
              SD.remove(sdFilePath);
              ota_result = ESP_FAIL;
            } else {
              Serial.printf("[OTA] Téléchargement SD réussi: %u bytes\n", (unsigned)written);
              
              // CHANGE: Flasher depuis SD
              Serial.println("[OTA] Flash depuis SD...");
              sdFile = SD.open(sdFilePath, FILE_READ);
              if (!sdFile) {
                Serial.printf("[OTA] ERREUR: Impossible d'ouvrir %s en lecture\n", sdFilePath);
                SD.remove(sdFilePath);
                ota_result = ESP_FAIL;
              } else {
                const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
                if (!next) {
                  Serial.println("[OTA] ERREUR: Aucune partition OTA disponible");
                  sdFile.close();
                  SD.remove(sdFilePath);
                  ota_result = ESP_FAIL;
                } else {
                  esp_ota_handle_t h = 0;
                  if (esp_ota_begin(next, (size_t)contentLength, &h) == ESP_OK) {
                    uint8_t buf[OTA_BUFFER_SIZE];
                    size_t flashed = 0;
                    bool flashOk = true;
                    
                    while (sdFile.available() && flashed < (size_t)contentLength && flashOk) {
                      size_t toRead = ((size_t)contentLength - flashed) > OTA_BUFFER_SIZE ? OTA_BUFFER_SIZE : ((size_t)contentLength - flashed);
                      size_t n = sdFile.read(buf, toRead);
                      
                      if (n == 0) break;
                      
                      if (esp_ota_write(h, buf, n) != ESP_OK) {
                        Serial.printf("[OTA] ERREUR: Écriture flash échouée à %u bytes\n", (unsigned)flashed);
                        flashOk = false;
                        break;
                      }
                      
                      flashed += n;
                      
                      // CHANGE: Publier progression
                      int pct = (flashed * 100) / contentLength;
                      if (flashed % (contentLength / 10) < OTA_BUFFER_SIZE || flashed == (size_t)contentLength) {
                        publishOtaProgress(version, "flash", pct);
                        Serial.printf("[OTA] Flash depuis SD: %u/%d bytes (%d%%)\n", (unsigned)flashed, contentLength, pct);
                      }
                      
                      vTaskDelay(pdMS_TO_TICKS(OTA_SD_FLASH_YIELD_MS));
                      taskYIELD();
                    }
                    
                    sdFile.close();
                    
                    if (flashOk && flashed == (size_t)contentLength && esp_ota_end(h) == ESP_OK && esp_ota_set_boot_partition(next) == ESP_OK) {
                      Serial.println("[OTA] Flash depuis SD réussi");
                      ota_result = ESP_OK;
                    } else {
                      Serial.printf("[OTA] ERREUR: Flash incomplet: %u/%d bytes\n", (unsigned)flashed, contentLength);
                      if (flashOk) esp_ota_abort(h);
                      ota_result = ESP_FAIL;
                    }
                    
                    // CHANGE: Supprimer le fichier SD après succès ou échec
                    SD.remove(sdFilePath);
                    Serial.printf("[OTA] Fichier SD %s supprimé\n", sdFilePath);
                  } else {
                    Serial.println("[OTA] ERREUR: esp_ota_begin échoué");
                    sdFile.close();
                    SD.remove(sdFilePath);
                    ota_result = ESP_FAIL;
                  }
                }
              }
            }
          }
        } else {
#endif
          // CHANGE: Mode direct (sans SD) - flash direct depuis HTTP stream
          const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
          if (next) {
            esp_ota_handle_t h = 0;
            if (esp_ota_begin(next, (size_t)contentLength, &h) == ESP_OK) {
              WiFiClient* stream = httpFallback.getStreamPtr();
              uint8_t buf[OTA_BUFFER_SIZE];
              size_t written = 0;
              bool ok = true;
              unsigned long lastProgressMs = millis();
              while (httpFallback.connected() && written < (size_t)contentLength && ok) {
                // CHANGE: Stall watchdog avec logging détaillé (written, avail, elapsed)
                unsigned long elapsed = millis() - lastProgressMs;
                if (elapsed > 30000) { // 30s sans progression
                  int avail = stream->available();
                  Serial.printf("[OTA] Stall détecté - written: %u/%d, avail: %d, elapsed: %lu ms\n", 
                    (unsigned)written, contentLength, avail, elapsed);
                }
                size_t toRead = (contentLength - (int)written) > (int)OTA_BUFFER_SIZE ? OTA_BUFFER_SIZE : (contentLength - (int)written);
                int n = stream->readBytes(buf, toRead);
                if (n <= 0) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }
                if (esp_ota_write(h, buf, (size_t)n) != ESP_OK) { ok = false; break; }
                written += (size_t)n;
                lastProgressMs = millis();
                
                // CHANGE: Publier progression
                int pct = (written * 100) / contentLength;
                if (written % (contentLength / 10) < OTA_BUFFER_SIZE || written == (size_t)contentLength) {
                  publishOtaProgress(version, "flash", pct);
                  Serial.printf("[OTA] Flash direct: %u/%d bytes (%d%%)\n", (unsigned)written, contentLength, pct);
                }
                
                taskYIELD();
              }
              httpFallback.end();
              if (ok && written == (size_t)contentLength && esp_ota_end(h) == ESP_OK && esp_ota_set_boot_partition(next) == ESP_OK) {
                ota_result = ESP_OK;
              } else {
                if (ok) esp_ota_abort(h);
              }
            }
          }
#ifdef HAS_SD
        }
#endif
      }
    }
    if (ota_result != ESP_OK) {
      httpFallback.end();
    }
  }

  if (ota_result != ESP_OK && !binUrlHttps) {
    Serial.println("[OTA] URL binaire en HTTP, téléchargement direct");
    // CHANGE: Activer les redirections pour téléchargement HTTP direct
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setRedirectLimit(5);
    http.begin(params->binUrl);
    // CHANGE: Augmenter les timeouts pour robustesse (connect 20s, read 60s)
    http.setConnectTimeout(20000);
    http.setTimeout(60000);
    
    // CHANGE: Logger le heap avant GET() pour diagnostic
    Serial.printf("[OTA] Heap libre avant HTTP direct GET: %u bytes\n", (unsigned)ESP.getFreeHeap());
    
    int code = http.GET();
    
    // CHANGE: Logging détaillé après GET()
    Serial.print("[OTA] HTTP direct - Code: ");
    Serial.println(code);
    if (code < 0) {
      Serial.print("[OTA] Erreur HTTP: ");
      Serial.println(http.errorToString(code));
    }
    String location = http.getLocation();
    if (location.length() > 0) {
      Serial.print("[OTA] Redirection vers: ");
      Serial.println(location);
    }
    
    if (code == HTTP_CODE_OK) {
      int contentLength = http.getSize();
      if (contentLength > 0) {
        const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
        if (next) {
          esp_ota_handle_t h = 0;
          if (esp_ota_begin(next, (size_t)contentLength, &h) == ESP_OK) {
            WiFiClient* stream = http.getStreamPtr();
            uint8_t buf[OTA_BUFFER_SIZE];
            size_t written = 0;
            bool ok = true;
            unsigned long lastProgressMs = millis();
            while (http.connected() && written < (size_t)contentLength && ok) {
              // CHANGE: Stall watchdog avec logging détaillé (written, avail, elapsed)
              unsigned long elapsed = millis() - lastProgressMs;
              if (elapsed > 30000) { // 30s sans progression
                int avail = stream->available();
                Serial.printf("[OTA] Stall détecté - written: %u, avail: %d, elapsed: %lu ms\n", 
                  (unsigned)written, avail, elapsed);
              }
              size_t toRead = (contentLength - (int)written) > (int)OTA_BUFFER_SIZE ? OTA_BUFFER_SIZE : (contentLength - (int)written);
              int n = stream->readBytes(buf, toRead);
              if (n <= 0) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }
              if (esp_ota_write(h, buf, (size_t)n) != ESP_OK) { ok = false; break; }
              written += (size_t)n;
              lastProgressMs = millis();
              taskYIELD();
            }
            http.end();
            if (ok && written == (size_t)contentLength && esp_ota_end(h) == ESP_OK && esp_ota_set_boot_partition(next) == ESP_OK) {
              ota_result = ESP_OK;
            } else {
              if (ok) esp_ota_abort(h);
            }
          }
        }
      }
    }
    if (ota_result != ESP_OK) {
      http.end();
    }
  }

  if (ota_result == ESP_OK) {
    Serial.println("[OTA] Mise à jour écrite avec succès (esp_https_ota). Envoi PubNub puis redémarrage...");
    
    // CHANGE: Logger les partitions courante et next pour diagnostic
    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* boot = esp_ota_get_boot_partition();
    if (running) {
      Serial.printf("[OTA] Partition courante: %s (offset 0x%x)\n", running->label, (unsigned)running->address);
    }
    if (boot) {
      Serial.printf("[OTA] Partition de boot: %s (offset 0x%x)\n", boot->label, (unsigned)boot->address);
    }
    
    // CHANGE: TODO - Marquer l'app comme valide au prochain boot pour éviter le rollback automatique
    // Après esp_restart(), au démarrage de la nouvelle partition, appeler:
    // esp_ota_mark_app_valid_cancel_rollback() dans init_manager.cpp ou main.cpp
    // API exacte: esp_err_t esp_ota_mark_app_valid_cancel_rollback(void)
    // Cela doit être fait après vérification que l'app démarre correctement (ex: après init WiFi/LED)
    
    char doneMsg[128];
    snprintf(doneMsg, sizeof(doneMsg), "{\"type\":\"firmware-update-done\",\"version\":\"%s\"}", version);
    if (PubNubManager::isInitialized()) {
      PubNubManager::publish(doneMsg);
      vTaskDelay(pdMS_TO_TICKS(2000));
    }
#ifdef HAS_LED
    if (LEDManager::isInitialized()) {
      LEDManager::setEffect(LED_EFFECT_NONE);
    }
#endif
    delete params;
    esp_restart();
    vTaskDelete(nullptr);
    return;
  }

  Serial.printf("[OTA] OTA échoué: %s (0x%x)\n", esp_err_to_name(ota_result), (unsigned)ota_result);
  char errMsg[160];
  snprintf(errMsg, sizeof(errMsg), "{\"type\":\"firmware-update-failed\",\"version\":\"%s\",\"error\":\"ota-%s\"}", version, esp_err_to_name(ota_result));
  if (PubNubManager::isInitialized()) {
    PubNubManager::publish(errMsg);
  }
#ifdef HAS_LED
  if (LEDManager::isInitialized()) {
    LEDManager::setEffect(LED_EFFECT_NONE);
  }
#endif
  delete params;
  vTaskDelete(nullptr);
  return;
}

bool OtaManager::start(const char* model, const char* version) {
  if (model == nullptr || version == nullptr || strlen(version) == 0) {
    Serial.println("[OTA] model ou version invalide");
    return false;
  }

  if (!WiFiManager::isConnected()) {
    Serial.println("[OTA] WiFi non connecté");
    return false;
  }

  char macStr[20];
  char macClean[13];
  macClean[0] = '\0';
  if (getMacAddressString(macStr, sizeof(macStr))) {
    for (size_t i = 0, j = 0; macStr[i] && j < 12; i++) {
      if (macStr[i] != ':') macClean[j++] = (char)toupper((unsigned char)macStr[i]);
    }
    macClean[12] = '\0';
  }

  char downloadUrl[512];
  if (macClean[0] != '\0') {
    snprintf(downloadUrl, sizeof(downloadUrl),
      "%s/api/firmware/download?model=%s&version=%s&mac=%s",
      API_BASE_URL, model, version, macClean);
  } else {
    snprintf(downloadUrl, sizeof(downloadUrl),
      "%s/api/firmware/download?model=%s&version=%s",
      API_BASE_URL, model, version);
  }

  OtaTaskParams* params = new OtaTaskParams();
  if (params == nullptr) {
    Serial.println("[OTA] Erreur allocation params");
    return false;
  }

  strncpy(params->downloadUrl, downloadUrl, sizeof(params->downloadUrl) - 1);
  params->downloadUrl[sizeof(params->downloadUrl) - 1] = '\0';
  strncpy(params->version, version, sizeof(params->version) - 1);
  params->version[sizeof(params->version) - 1] = '\0';
  strncpy(params->model, model, sizeof(params->model) - 1);
  params->model[sizeof(params->model) - 1] = '\0';
  params->binUrl[0] = '\0';
  params->fallbackUrl[0] = '\0';

  size_t freeHeap = ESP.getFreeHeap();
  Serial.printf("[OTA] Heap libre avant création tâche: %u bytes (stack OTA: %u)\n", (unsigned)freeHeap, (unsigned)OTA_TASK_STACK_SIZE);

  BaseType_t created = xTaskCreatePinnedToCore(
    otaTaskFunction,
    "ota_task",
    OTA_TASK_STACK_SIZE,
    params,
    PRIORITY_OTA,
    nullptr,
    CORE_OTA
  );

  if (created != pdPASS) {
    Serial.println("[OTA] Erreur création tâche (heap insuffisant ou stack trop grande)");
    delete params;
    return false;
  }

  Serial.println("[OTA] Tâche OTA démarrée");
  return true;
}

#else // !HAS_WIFI

bool OtaManager::start(const char* model, const char* version) {
  (void)model;
  (void)version;
  return false;
}

#endif // HAS_WIFI
