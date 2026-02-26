#include "wifi_manager.h"
#include "models/model_config.h"
#include "common/config/core_config.h"
#include "common/managers/init/init_manager.h"
#include "common/managers/sd/sd_manager.h"
#include "common/managers/log/log_manager.h"

#ifdef HAS_WIFI
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

#ifdef HAS_PUBNUB
#include "common/managers/pubnub/pubnub_manager.h"
#include "common/managers/ota/ota_manager.h"
#endif

#ifdef HAS_RTC
#include "common/managers/rtc/rtc_manager.h"
#endif

// Routes de synchronisation de configuration (spécifiques au modèle)
#include "models/model_config_sync_routes.h"

// Variables statiques
bool WiFiManager::initialized = false;
bool WiFiManager::available = false;
WiFiConnectionStatus WiFiManager::connectionStatus = WIFI_STATUS_DISCONNECTED;
char WiFiManager::currentSSID[64] = "";

// Thread de retry
TaskHandle_t WiFiManager::retryTaskHandle = nullptr;
bool WiFiManager::retryThreadRunning = false;
unsigned long WiFiManager::retryStartTime = 0;

bool WiFiManager::init() {
  if (initialized) {
    return available;
  }
  
  initialized = true;
  available = false;
  connectionStatus = WIFI_STATUS_DISCONNECTED;
  currentSSID[0] = '\0';
  
#ifndef HAS_WIFI
  LogManager::info("[WIFI] WiFi non disponible sur ce modele");
  return false;
#else
  // Configurer le WiFi en mode Station (client)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  available = true;
  return true;
#endif
}

bool WiFiManager::isAvailable() {
  return initialized && available;
}

bool WiFiManager::isInitialized() {
  return initialized;
}

bool WiFiManager::connect() {
#ifndef HAS_WIFI
  return false;
#else
  if (!available) {
    LogManager::error("[WIFI] WiFi non initialise");
    return false;
  }
  
  // Récupérer la config depuis InitManager
  const SDConfig& config = InitManager::getConfig();
  
  // Vérifier si les identifiants WiFi sont configurés
  if (strlen(config.wifi_ssid) == 0) {
    LogManager::warning("[WIFI] Aucun SSID configure dans config.json");
    connectionStatus = WIFI_STATUS_DISCONNECTED;
    return false;
  }
  
  LogManager::info("[WIFI] Connexion au reseau: %s", config.wifi_ssid);
  
  return connect(config.wifi_ssid, config.wifi_password, DEFAULT_CONNECT_TIMEOUT_MS);
#endif
}

bool WiFiManager::connect(const char* ssid, const char* password, uint32_t timeoutMs) {
#ifndef HAS_WIFI
  return false;
#else
  if (!available) {
    LogManager::error("[WIFI] WiFi non initialise");
    return false;
  }
  
  if (ssid == nullptr || strlen(ssid) == 0) {
    LogManager::error("[WIFI] SSID invalide");
    connectionStatus = WIFI_STATUS_CONNECTION_FAILED;
    return false;
  }
  
  // Sauvegarder le SSID
  strncpy(currentSSID, ssid, sizeof(currentSSID) - 1);
  currentSSID[sizeof(currentSSID) - 1] = '\0';
  
  connectionStatus = WIFI_STATUS_CONNECTING;
  
  // Démarrer la connexion
  WiFi.begin(ssid, password);
  
  // Attendre la connexion avec timeout
  unsigned long startTime = millis();
  unsigned long lastRetryTime = startTime;
  const unsigned long RETRY_BACKOFF_MS = 4000;   // Pause toutes les 4 s si l'AP refuse (évite "Association refused temporarily")
  const unsigned long BACKOFF_WAIT_MS = 3000;    // Attendre 3 s avant de réessayer (respecte le backoff de l'AP)
  int dotCount = 0;
  wl_status_t lastMeaningfulStatus = WL_DISCONNECTED; // Dernier statut avant disconnect/begin

  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - startTime >= timeoutMs) {
      LogManager::error("[WIFI] Timeout de connexion");
      // Afficher la raison pour aider au diagnostic (box vs partage teléphone)
      switch (lastMeaningfulStatus) {
        case WL_NO_SSID_AVAIL:
          LogManager::error("[WIFI] Raison: reseau non trouve (WL_NO_SSID_AVAIL). Verifiez: SSID exact (accents e/é), 2.4 GHz, portee.");
          break;
        case WL_CONNECT_FAILED:
          LogManager::error("[WIFI] Raison: echec auth. Verifiez: mot de passe, securite WPA2 (pas WPA3 seul - Samsung S24/Android 14 utilise WPA3 par defaut).");
          break;
        case WL_DISCONNECTED:
        case WL_CONNECTION_LOST:
          LogManager::error("[WIFI] Raison: connexion interrompue. Verifiez: signal, isolation client desactivee sur la box.");
          break;
        default:
          LogManager::error("[WIFI] Raison: code status=%d. Voir doc ESP32 WiFi (2.4 GHz, WPA2).", (int)lastMeaningfulStatus);
          break;
      }
      connectionStatus = WIFI_STATUS_CONNECTION_FAILED;
      return false;
    }
    
    // Si l'AP refuse (Association refused temporarily) : faire une pause puis réessayer au lieu de bombarder
    if (millis() - lastRetryTime >= RETRY_BACKOFF_MS) {
      // Sauvegarder le statut AVANT disconnect (apres begin il passe a WL_NO_SSID_AVAIL transitoire)
      lastMeaningfulStatus = WiFi.status();
      WiFi.disconnect();
      delay(BACKOFF_WAIT_MS);
      WiFi.begin(ssid, password);
      lastRetryTime = millis();
    }
    
    delay(500);
    if (Serial) Serial.print(".");
    dotCount++;
    
    if (dotCount >= 40) {
      if (Serial) Serial.println();
      dotCount = 0;
    }
  }
  
  if (Serial) Serial.println();
  connectionStatus = WIFI_STATUS_CONNECTED;
  
  // Arrêter le thread de retry si actif (connexion réussie)
  if (retryThreadRunning) {
    stopRetryThread();
  }
  
  LogManager::info("[WIFI] ========================================");
  LogManager::info("[WIFI] Connecte avec succes !");
  LogManager::info("[WIFI] SSID: %s", ssid);
  LogManager::info("[WIFI] Adresse IP: %s", WiFi.localIP().toString().c_str());
  LogManager::info("[WIFI] Force du signal: %d dBm", WiFi.RSSI());
  LogManager::info("[WIFI] ========================================");
  
  // Déclencher la connexion PubNub si disponible (sauf pendant OTA)
  #ifdef HAS_PUBNUB
  if (PubNubManager::isInitialized() && !PubNubManager::isConnected()
      && !OTAManager::isOtaInProgress()) {
    LogManager::info("[WIFI] Connexion automatique PubNub...");
    PubNubManager::connect();
  }
  #endif
  
  // Synchroniser la configuration via les routes spécifiques au modèle
  ModelConfigSyncRoutes::onWiFiConnected();

  #ifdef HAS_PUBNUB
  OTAManager::publishLastOtaErrorIfAny();
  #endif
  
  return true;
#endif
}

#ifdef HAS_WIFI
struct ConnectAsyncParams {
  char ssid[64];
  char password[64];
  uint32_t timeoutMs;
  void (*callback)(bool success, void* userData);
  void* userData;
};
#endif

void WiFiManager::connectAsync(const char* ssid, const char* password, uint32_t timeoutMs,
                              void (*callback)(bool success, void* userData), void* userData) {
#ifndef HAS_WIFI
  if (callback) callback(false, userData);
  return;
#else
  if (!available || !ssid || !callback) {
    if (callback) callback(false, userData);
    return;
  }
  ConnectAsyncParams* params = new ConnectAsyncParams();
  strncpy(params->ssid, ssid, sizeof(params->ssid) - 1);
  params->ssid[sizeof(params->ssid) - 1] = '\0';
  strncpy(params->password, password ? password : "", sizeof(params->password) - 1);
  params->password[sizeof(params->password) - 1] = '\0';
  params->timeoutMs = timeoutMs;
  params->callback = callback;
  params->userData = userData;
  xTaskCreatePinnedToCore(
    connectTaskFunction,
    "WiFiConnect",
    STACK_SIZE_WIFI_CONNECT,
    params,
    PRIORITY_WIFI_RETRY,
    nullptr,
    CORE_WIFI_RETRY
  );
#endif
}

void WiFiManager::connectTaskFunction(void* parameter) {
#ifdef HAS_WIFI
  ConnectAsyncParams* params = static_cast<ConnectAsyncParams*>(parameter);
  bool success = connect(params->ssid, params->password[0] ? params->password : nullptr, params->timeoutMs);
  void (*cb)(bool, void*) = params->callback;
  void* ud = params->userData;
  delete params;
  if (cb) cb(success, ud);
  vTaskDelete(nullptr);
#endif
}

void WiFiManager::disconnect() {
#ifdef HAS_WIFI
  if (!available) {
    return;
  }
  
  // Arrêter le thread de retry si actif
  if (retryThreadRunning) {
    stopRetryThread();
  }
  
  WiFi.disconnect();
  connectionStatus = WIFI_STATUS_DISCONNECTED;
  currentSSID[0] = '\0';
  LogManager::info("[WIFI] Deconnecte");
#endif
}

bool WiFiManager::isConnected() {
#ifdef HAS_WIFI
  if (!available) {
    return false;
  }
  
  bool connected = (WiFi.status() == WL_CONNECTED);
  
  // Mettre à jour le statut si nécessaire
  if (connected && connectionStatus != WIFI_STATUS_CONNECTED) {
    connectionStatus = WIFI_STATUS_CONNECTED;
  } else if (!connected && connectionStatus == WIFI_STATUS_CONNECTED) {
    connectionStatus = WIFI_STATUS_DISCONNECTED;
  }
  
  return connected;
#else
  return false;
#endif
}

WiFiConnectionStatus WiFiManager::getConnectionStatus() {
  return connectionStatus;
}

String WiFiManager::getLocalIP() {
#ifdef HAS_WIFI
  if (!available || !isConnected()) {
    return "0.0.0.0";
  }
  return WiFi.localIP().toString();
#else
  return "0.0.0.0";
#endif
}

String WiFiManager::getSSID() {
#ifdef HAS_WIFI
  if (!available || !isConnected()) {
    return "";
  }
  return String(currentSSID);
#else
  return "";
#endif
}

int WiFiManager::getRSSI() {
#ifdef HAS_WIFI
  if (!available || !isConnected()) {
    return 0;
  }
  return WiFi.RSSI();
#else
  return 0;
#endif
}

void WiFiManager::printInfo() {
  LogManager::info("[WIFI] ========== Info WiFi ==========");
  
#ifndef HAS_WIFI
  LogManager::info("[WIFI] WiFi non disponible sur ce modele");
#else
  if (!initialized) {
    LogManager::info("[WIFI] WiFi non initialise");
  } else if (!available) {
    LogManager::info("[WIFI] WiFi non disponible");
  } else {
    const char* statusStr = "?";
    switch (connectionStatus) {
      case WIFI_STATUS_DISCONNECTED: statusStr = "Deconnecte"; break;
      case WIFI_STATUS_CONNECTING: statusStr = "Connexion en cours..."; break;
      case WIFI_STATUS_CONNECTED:
        LogManager::info("[WIFI] Statut: Connecte");
        LogManager::info("[WIFI] SSID: %s", currentSSID);
        LogManager::info("[WIFI] IP: %s", WiFi.localIP().toString().c_str());
        LogManager::info("[WIFI] RSSI: %d dBm", WiFi.RSSI());
        statusStr = nullptr;
        break;
      case WIFI_STATUS_CONNECTION_FAILED: statusStr = "Echec de connexion"; break;
    }
    if (statusStr != nullptr) {
      LogManager::info("[WIFI] Statut: %s", statusStr);
    }
    LogManager::info("[WIFI] Thread retry actif: %s", retryThreadRunning ? "Oui" : "Non");
  }
#endif
  
  LogManager::info("[WIFI] ================================");
}

void WiFiManager::startRetryThread() {
#ifdef HAS_WIFI
  if (!available || retryThreadRunning) {
    return;
  }
  
  // Vérifier si déjà connecté
  if (isConnected()) {
    return;
  }
  
  // Vérifier qu'il y a un SSID configuré
  const SDConfig& config = InitManager::getConfig();
  if (strlen(config.wifi_ssid) == 0) {
    LogManager::warning("[WIFI] Pas de SSID configure, retry impossible");
    return;
  }
  
  LogManager::info("[WIFI] Demarrage du thread de retry automatique...");
  retryStartTime = millis();
  retryThreadRunning = true;
  
  // Créer le thread FreeRTOS sur Core 0 (même core que WiFi stack)
  LogManager::debug("[WIFI-RETRY] Core=%d, Priority=%d, Stack=%d", RETRY_TASK_CORE, RETRY_TASK_PRIORITY, RETRY_STACK_SIZE);
  BaseType_t result = xTaskCreatePinnedToCore(
    retryThreadFunction,  // Fonction du thread
    "WiFiRetryTask",     // Nom du thread
    RETRY_STACK_SIZE,    // Taille de la stack
    nullptr,             // Paramètre
    RETRY_TASK_PRIORITY, // Priorité
    &retryTaskHandle,    // Handle
    RETRY_TASK_CORE      // Core 0 avec WiFi stack (configuré dans core_config.h)
  );
  
  if (result != pdPASS) {
    LogManager::error("[WIFI] Erreur creation thread retry");
    retryThreadRunning = false;
    retryTaskHandle = nullptr;
  }
#endif
}

void WiFiManager::stopRetryThread() {
#ifdef HAS_WIFI
  if (!retryThreadRunning) {
    return;
  }
  
  retryThreadRunning = false;
  
  if (retryTaskHandle != nullptr) {
    vTaskDelay(pdMS_TO_TICKS(100)); // Laisser le temps au thread de s'arrêter
    vTaskDelete(retryTaskHandle);
    retryTaskHandle = nullptr;
  }
  
  LogManager::info("[WIFI] Thread retry arrete");
#endif
}

bool WiFiManager::isRetryThreadActive() {
#ifdef HAS_WIFI
  return retryThreadRunning;
#else
  return false;
#endif
}

void WiFiManager::retryThreadFunction(void* parameter) {
#ifdef HAS_WIFI
  Serial.println("[WIFI-RETRY] Thread actif");
  
  const SDConfig& config = InitManager::getConfig();
  uint32_t retryDelay = RETRY_INITIAL_DELAY_MS; // Commence à 5 secondes
  int attemptCount = 0;
  
  while (retryThreadRunning) {
    // Vérifier si on a dépassé la durée maximale (1 minute)
    if (millis() - retryStartTime >= RETRY_MAX_DURATION_MS) {
      LogManager::info("[WIFI-RETRY] Duree maximale atteinte (1 minute), arret du retry");
      retryThreadRunning = false;
      break;
    }
    
    // Vérifier si on est déjà connecté (peut arriver si connecté manuellement)
    if (WiFiManager::isConnected()) {
      LogManager::info("[WIFI-RETRY] WiFi connecte, arret du retry");
      
      // Synchroniser l'heure RTC via NTP
      #ifdef HAS_RTC
      RTCManager::autoSyncIfNeeded();
      #endif
      
      // Déclencher la connexion PubNub si disponible (sauf pendant OTA)
      #ifdef HAS_PUBNUB
      if (PubNubManager::isInitialized() && !PubNubManager::isConnected()
          && !OTAManager::isOtaInProgress()) {
        LogManager::info("[WIFI-RETRY] Connexion automatique PubNub...");
        PubNubManager::connect();
      }
      #endif
      
      // Synchroniser la configuration via les routes spécifiques au modèle
      ModelConfigSyncRoutes::onWiFiConnected();
      
      retryThreadRunning = false;
      break;
    }
    
    // Tenter de se connecter
    attemptCount++;
    Serial.print("[WIFI-RETRY] Tentative ");
    Serial.print(attemptCount);
    Serial.print(" (delai: ");
    Serial.print(retryDelay / 1000);
    Serial.println("s)");
    
    // Utiliser connect() avec les paramètres de la config
    // Cette méthode va bloquer jusqu'à 15 secondes (timeout)
    if (WiFiManager::connect(config.wifi_ssid, config.wifi_password, DEFAULT_CONNECT_TIMEOUT_MS)) {
      LogManager::info("[WIFI-RETRY] Connexion reussie !");
      
      // Synchroniser l'heure RTC via NTP
      #ifdef HAS_RTC
      RTCManager::autoSyncIfNeeded();
      #endif
      
      // Déclencher la connexion PubNub si disponible (sauf pendant OTA)
      #ifdef HAS_PUBNUB
      if (PubNubManager::isInitialized() && !PubNubManager::isConnected()
          && !OTAManager::isOtaInProgress()) {
        LogManager::info("[WIFI-RETRY] Connexion automatique PubNub...");
        PubNubManager::connect();
      }
      #endif
      
      // Synchroniser la configuration via les routes spécifiques au modèle
      ModelConfigSyncRoutes::onWiFiConnected();
      
      retryThreadRunning = false;
      break;
    }
    
    // Calculer le prochain délai avec backoff exponentiel
    // 5s, 10s, 15s, 20s, 30s, 40s, 50s, 60s max
    if (attemptCount == 1) {
      retryDelay = 10000;  // 10s
    } else if (attemptCount == 2) {
      retryDelay = 15000;   // 15s
    } else if (attemptCount == 3) {
      retryDelay = 20000;   // 20s
    } else if (attemptCount == 4) {
      retryDelay = 30000;   // 30s
    } else if (attemptCount == 5) {
      retryDelay = 40000;   // 40s
    } else if (attemptCount == 6) {
      retryDelay = 50000;   // 50s
    } else {
      retryDelay = RETRY_MAX_DELAY_MS; // 60s max
    }
    
    // Limiter le délai au maximum
    if (retryDelay > RETRY_MAX_DELAY_MS) {
      retryDelay = RETRY_MAX_DELAY_MS;
    }
    
    // Attendre avant la prochaine tentative
    vTaskDelay(pdMS_TO_TICKS(retryDelay));
  }
  
  retryThreadRunning = false;
  LogManager::info("[WIFI-RETRY] Thread arrete");
  vTaskDelete(nullptr);
#endif
}
