#include "wifi_manager.h"
#include "../../../model_config.h"
#include "../../config/core_config.h"
#include "../init/init_manager.h"
#include "../sd/sd_manager.h"

#ifdef HAS_WIFI
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

#ifdef HAS_PUBNUB
#include "../pubnub/pubnub_manager.h"
#endif

#ifdef HAS_RTC
#include "../rtc/rtc_manager.h"
#endif

// Routes de synchronisation de configuration (spécifiques au modèle)
#include "../../../model_config_sync_routes.h"

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
  Serial.println("[WIFI] WiFi non disponible sur ce modele");
  return false;
#else
  Serial.println("[WIFI] Initialisation du WiFi...");
  
  // Configurer le WiFi en mode Station (client)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  available = true;
  Serial.println("[WIFI] WiFi initialise (mode Station)");
  
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
    Serial.println("[WIFI] ERREUR: WiFi non initialise");
    return false;
  }
  
  // Récupérer la config depuis InitManager
  const SDConfig& config = InitManager::getConfig();
  
  // Vérifier si les identifiants WiFi sont configurés
  if (strlen(config.wifi_ssid) == 0) {
    Serial.println("[WIFI] Aucun SSID configure dans config.json");
    connectionStatus = WIFI_STATUS_DISCONNECTED;
    return false;
  }
  
  Serial.print("[WIFI] Connexion au reseau: ");
  Serial.println(config.wifi_ssid);
  
  return connect(config.wifi_ssid, config.wifi_password, DEFAULT_CONNECT_TIMEOUT_MS);
#endif
}

bool WiFiManager::connect(const char* ssid, const char* password, uint32_t timeoutMs) {
#ifndef HAS_WIFI
  return false;
#else
  if (!available) {
    Serial.println("[WIFI] ERREUR: WiFi non initialise");
    return false;
  }
  
  if (ssid == nullptr || strlen(ssid) == 0) {
    Serial.println("[WIFI] ERREUR: SSID invalide");
    connectionStatus = WIFI_STATUS_CONNECTION_FAILED;
    return false;
  }
  
  // Sauvegarder le SSID
  strncpy(currentSSID, ssid, sizeof(currentSSID) - 1);
  currentSSID[sizeof(currentSSID) - 1] = '\0';
  
  connectionStatus = WIFI_STATUS_CONNECTING;
  
  Serial.print("[WIFI] Connexion a: ");
  Serial.println(ssid);
  
  // Démarrer la connexion
  WiFi.begin(ssid, password);
  
  // Attendre la connexion avec timeout
  unsigned long startTime = millis();
  int dotCount = 0;
  
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - startTime >= timeoutMs) {
      Serial.println();
      Serial.println("[WIFI] ERREUR: Timeout de connexion");
      // Afficher la raison pour aider au diagnostic (box vs partage teléphone)
      wl_status_t lastStatus = WiFi.status();
      switch (lastStatus) {
        case WL_NO_SSID_AVAIL:
          Serial.println("[WIFI] Raison: reseau non trouve. Verifiez: SSID correct, box en 2.4 GHz (ESP32 ne fait pas 5 GHz), portee.");
          break;
        case WL_CONNECT_FAILED:
          Serial.println("[WIFI] Raison: echec auth. Verifiez: mot de passe, box en WPA2 ou WPA2/WPA3 mixte (pas WPA3 seul).");
          break;
        case WL_DISCONNECTED:
        case WL_CONNECTION_LOST:
          Serial.println("[WIFI] Raison: connexion interrompue. Verifiez: signal, isolation client desactivee sur la box.");
          break;
        default:
          Serial.printf("[WIFI] Raison: code status=%d. Voir doc ESP32 WiFi (2.4 GHz, WPA2).\n", (int)lastStatus);
          break;
      }
      connectionStatus = WIFI_STATUS_CONNECTION_FAILED;
      return false;
    }
    
    delay(500);
    Serial.print(".");
    dotCount++;
    
    if (dotCount >= 40) {
      Serial.println();
      dotCount = 0;
    }
  }
  
  Serial.println();
  connectionStatus = WIFI_STATUS_CONNECTED;
  
  // Arrêter le thread de retry si actif (connexion réussie)
  if (retryThreadRunning) {
    stopRetryThread();
  }
  
  Serial.println("[WIFI] ========================================");
  Serial.println("[WIFI] Connecte avec succes !");
  Serial.print("[WIFI] SSID: ");
  Serial.println(ssid);
  Serial.print("[WIFI] Adresse IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("[WIFI] Force du signal: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
  Serial.println("[WIFI] ========================================");
  
  // Déclencher la connexion PubNub si disponible
  #ifdef HAS_PUBNUB
  if (PubNubManager::isInitialized() && !PubNubManager::isConnected()) {
    Serial.println("[WIFI] Connexion automatique PubNub...");
    PubNubManager::connect();
  }
  #endif
  
  // Synchroniser la configuration via les routes spécifiques au modèle
  ModelConfigSyncRoutes::onWiFiConnected();
  
  return true;
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
  Serial.println("[WIFI] Deconnecte");
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
  Serial.println("[WIFI] ========== Info WiFi ==========");
  
#ifndef HAS_WIFI
  Serial.println("[WIFI] WiFi non disponible sur ce modele");
#else
  if (!initialized) {
    Serial.println("[WIFI] WiFi non initialise");
  } else if (!available) {
    Serial.println("[WIFI] WiFi non disponible");
  } else {
    Serial.print("[WIFI] Statut: ");
    switch (connectionStatus) {
      case WIFI_STATUS_DISCONNECTED:
        Serial.println("Deconnecte");
        break;
      case WIFI_STATUS_CONNECTING:
        Serial.println("Connexion en cours...");
        break;
      case WIFI_STATUS_CONNECTED:
        Serial.println("Connecte");
        Serial.print("[WIFI] SSID: ");
        Serial.println(currentSSID);
        Serial.print("[WIFI] IP: ");
        Serial.println(WiFi.localIP());
        Serial.print("[WIFI] RSSI: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
        break;
      case WIFI_STATUS_CONNECTION_FAILED:
        Serial.println("Echec de connexion");
        break;
    }
    
    Serial.print("[WIFI] Thread retry actif: ");
    Serial.println(retryThreadRunning ? "Oui" : "Non");
  }
#endif
  
  Serial.println("[WIFI] ================================");
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
    Serial.println("[WIFI] Pas de SSID configure, retry impossible");
    return;
  }
  
  Serial.println("[WIFI] Demarrage du thread de retry automatique...");
  retryStartTime = millis();
  retryThreadRunning = true;
  
  // Créer le thread FreeRTOS sur Core 0 (même core que WiFi stack)
  Serial.printf("[WIFI-RETRY] Core=%d, Priority=%d, Stack=%d\n", RETRY_TASK_CORE, RETRY_TASK_PRIORITY, RETRY_STACK_SIZE);
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
    Serial.println("[WIFI] Erreur creation thread retry");
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
  
  Serial.println("[WIFI] Thread retry arrete");
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
      Serial.println("[WIFI-RETRY] Duree maximale atteinte (1 minute), arret du retry");
      retryThreadRunning = false;
      break;
    }
    
    // Vérifier si on est déjà connecté (peut arriver si connecté manuellement)
    if (WiFiManager::isConnected()) {
      Serial.println("[WIFI-RETRY] WiFi connecte, arret du retry");
      
      // Synchroniser l'heure RTC via NTP
      #ifdef HAS_RTC
      RTCManager::autoSyncIfNeeded();
      #endif
      
      // Déclencher la connexion PubNub si disponible
      #ifdef HAS_PUBNUB
      if (PubNubManager::isInitialized() && !PubNubManager::isConnected()) {
        Serial.println("[WIFI-RETRY] Connexion automatique PubNub...");
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
      Serial.println("[WIFI-RETRY] Connexion reussie !");
      
      // Synchroniser l'heure RTC via NTP
      #ifdef HAS_RTC
      RTCManager::autoSyncIfNeeded();
      #endif
      
      // Déclencher la connexion PubNub si disponible
      #ifdef HAS_PUBNUB
      if (PubNubManager::isInitialized() && !PubNubManager::isConnected()) {
        Serial.println("[WIFI-RETRY] Connexion automatique PubNub...");
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
  Serial.println("[WIFI-RETRY] Thread arrete");
  vTaskDelete(nullptr);
#endif
}
