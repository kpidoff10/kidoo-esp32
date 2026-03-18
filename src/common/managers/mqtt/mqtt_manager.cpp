#include "mqtt_manager.h"
#include "models/model_config.h"
#include "common/config/core_config.h"
#include "common/managers/log/log_manager.h"

#ifdef HAS_MQTT

#include <esp_mac.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "common/managers/wifi/wifi_manager.h"
#include "common/managers/serial/serial_commands.h"
#include "common/managers/init/init_manager.h"
#include "common/managers/rtc/rtc_manager.h"
#include "common/managers/device_key/device_key_manager.h"
#include "models/model_mqtt_routes.h"

// Variables statiques
bool MqttManager::initialized = false;
bool MqttManager::connected = false;
bool MqttManager::threadRunning = false;
char MqttManager::cmdTopic[80] = "";
char MqttManager::telemetryTopic[80] = "";
char MqttManager::clientId[64] = "";
WiFiClient MqttManager::espClient;
PubSubClient MqttManager::mqttClient(espClient);
TaskHandle_t MqttManager::taskHandle = nullptr;
QueueHandle_t MqttManager::publishQueue = nullptr;

// Credentials MQTT (récupérés du serveur)
static char mqttUsername[64] = "kidoo_app";
static char mqttPassword[256] = "";
static char mqttBrokerUrl[256] = "";  // URL du broker (ex: mqtt://45.10.161.70:1883)
static char mqttBrokerHost[256] = "";  // Host parsé de mqttBrokerUrl
static uint16_t mqttBrokerPort = 1883;  // Port parsé de mqttBrokerUrl

// Structure pour les messages à publier
struct PublishMessage {
  char message[512];
};

/**
 * Parser une URL MQTT pour extraire le host et le port
 * Format attendu: mqtt://host:port ou ws://host:port
 */
static bool parseMqttUrl(const char* url, char* host, int hostSize, uint16_t* port) {
  if (!url || strlen(url) == 0) return false;

  // Trouver le début du host (après mqtt:// ou ws://)
  const char* hostStart = strstr(url, "://");
  if (!hostStart) return false;
  hostStart += 3;

  // Trouver le port (après :)
  const char* portStart = strchr(hostStart, ':');
  if (!portStart) {
    // Pas de port, utiliser 1883 par défaut
    strncpy(host, hostStart, hostSize - 1);
    host[hostSize - 1] = '\0';
    *port = 1883;
    return true;
  }

  // Copier le host
  int hostLen = portStart - hostStart;
  if (hostLen >= hostSize) hostLen = hostSize - 1;
  strncpy(host, hostStart, hostLen);
  host[hostLen] = '\0';

  // Parser le port
  *port = atoi(portStart + 1);
  return true;
}

/**
 * Récupérer les credentials MQTT du serveur via l'API (avec signature Ed25519)
 */
static bool fetchMqttCredentials(const char* mac) {
  if (!WiFiManager::isConnected()) {
    LogManager::warning("[MQTT] WiFi non connecte, impossible de recuperer les credentials");
    return false;
  }

  // Créer le message à signer: GET\n/api/devices/{MAC}/mqtt-token\n{TIMESTAMP}
  uint32_t timestamp = RTCManager::getUnixTime();
  char timestampStr[12];
  snprintf(timestampStr, sizeof(timestampStr), "%lu", (unsigned long)timestamp);

  char path[128];
  snprintf(path, sizeof(path), "/api/devices/%s/mqtt-token", mac);

  char message[512];
  snprintf(message, sizeof(message), "GET\n%s\n%s", path, timestampStr);

  // Signer le message avec la clé privée Ed25519
  char signatureB64[96] = {0};
  if (!DeviceKeyManager::signMessageBase64((const uint8_t*)message, strlen(message), signatureB64, sizeof(signatureB64))) {
    LogManager::error("[MQTT] Erreur signature device");
    return false;
  }

  // Construire l'URL et la requête
  char url[256];
  snprintf(url, sizeof(url), "%s/api/devices/%s/mqtt-token", API_BASE_URL, mac);

  LogManager::info("[MQTT] Recuperation credentials signees: %s", path);
  LogManager::info("[MQTT] URL: %s", url);
  LogManager::info("[MQTT] Signature: %.20s... (length=%d)", signatureB64, strlen(signatureB64));

  HTTPClient http;
  http.begin(url);
  http.addHeader("x-kidoo-signature", signatureB64);
  http.addHeader("x-kidoo-timestamp", timestampStr);

  int httpCode = http.GET();

  LogManager::info("[MQTT] HTTP Response Code: %d", httpCode);

  if (httpCode == 200) {
    String payload = http.getString();
    LogManager::info("[MQTT] Response payload: %s", payload.c_str());

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      LogManager::error("[MQTT] JSON parse error: %s", error.c_str());
    } else if (doc["data"]["mqttPassword"].is<const char*>() && doc["data"]["mqttUrl"].is<const char*>()) {
      strncpy(mqttPassword, doc["data"]["mqttPassword"], sizeof(mqttPassword) - 1);
      strncpy(mqttBrokerUrl, doc["data"]["mqttUrl"], sizeof(mqttBrokerUrl) - 1);
      LogManager::info("[MQTT] Credentials recuperes avec succes");
      LogManager::info("[MQTT] Broker URL: %s", mqttBrokerUrl);

      // Parser l'URL pour extraire le host et le port
      if (parseMqttUrl(mqttBrokerUrl, mqttBrokerHost, sizeof(mqttBrokerHost), &mqttBrokerPort)) {
        LogManager::info("[MQTT] Broker parsé: %s:%d", mqttBrokerHost, mqttBrokerPort);
      } else {
        LogManager::warning("[MQTT] Erreur parsing URL du broker");
      }

      http.end();
      return true;
    } else {
      LogManager::warning("[MQTT] mqttPassword or mqttUrl not found or not a string in response");
    }
  } else {
    String errorPayload = http.getString();
    LogManager::warning("[MQTT] HTTP Error (code=%d): %s", httpCode, errorPayload.c_str());
  }

  http.end();
  return false;
}

bool MqttManager::init() {
  if (initialized) {
    return true;
  }

  // Construire les topics basés sur l'adresse MAC WiFi (unique par appareil)
  uint8_t mac[6];
  esp_err_t err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
  if (err != ESP_OK) {
    LogManager::warning("[MQTT] esp_read_mac() échoué (err=%d), utilisation de WiFi.macAddress()", err);
    WiFi.macAddress(mac);
  }

  // Recuperer les credentials MQTT du serveur
  char macStr[13];
  snprintf(macStr, sizeof(macStr), "%02X%02X%02X%02X%02X%02X",
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  fetchMqttCredentials(macStr);

  // Format : kidoo/{MAC}/cmd et kidoo/{MAC}/telemetry
  snprintf(cmdTopic, sizeof(cmdTopic), "kidoo/%02X%02X%02X%02X%02X%02X/cmd",
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  snprintf(telemetryTopic, sizeof(telemetryTopic), "kidoo/%02X%02X%02X%02X%02X%02X/telemetry",
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  snprintf(clientId, sizeof(clientId), "kidoo-%02X%02X%02X%02X%02X%02X",
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  LogManager::info("[MQTT] Topics construits avec MAC: cmd=%s, telemetry=%s", cmdTopic, telemetryTopic);

  // Configurer le client MQTT avec l'URL du serveur
  // Server is the only source of truth for broker config
  if (strlen(mqttBrokerHost) == 0) {
    LogManager::error("[MQTT] Broker host not fetched from server");
    return false;
  }

  LogManager::info("[MQTT] Configuration broker: %s:%d", mqttBrokerHost, mqttBrokerPort);

  mqttClient.setServer(mqttBrokerHost, mqttBrokerPort);
  mqttClient.setCallback(onMessage);
  mqttClient.setBufferSize(1024);  // Pour les gros messages JSON (get-info)
  mqttClient.setKeepAlive(60);  // Keep-alive TCP toutes les 60 secondes (détection perte WiFi)

  // Créer la file d'attente pour les publications
  publishQueue = xQueueCreate(PUBLISH_QUEUE_SIZE, sizeof(PublishMessage));
  if (publishQueue == nullptr) {
    LogManager::error("[MQTT] Erreur creation queue");
    return false;
  }

  initialized = true;
  return true;
}

bool MqttManager::connect() {
  LogManager::info("[MQTT] connect() appelé - initialized: %d, threadRunning: %d, WiFi: %d",
    initialized, threadRunning, WiFiManager::isConnected());

  if (!initialized) {
    LogManager::error("[MQTT] Non initialise");
    return false;
  }

  // Vérifier que le WiFi est connecté
  if (!WiFiManager::isConnected()) {
    LogManager::warning("[MQTT] WiFi non connecte");
    return false;
  }

  // Si le thread tourne déjà, on est déjà connecté
  if (threadRunning) {
    LogManager::debug("[MQTT] Deja connecte (threadRunning=true)");
    return true;
  }

  // Si taskHandle existe mais threadRunning est false, nettoyer d'abord
  if (taskHandle != nullptr) {
    LogManager::info("[MQTT] Nettoyage d'un ancien thread...");
    vTaskDelete(taskHandle);
    taskHandle = nullptr;
  }

  // Créer le thread FreeRTOS sur Core MQTT
  LogManager::debug("[MQTT] Core=%d, Priority=%d, Stack=%d", CORE_MQTT, PRIORITY_MQTT, STACK_SIZE_MQTT);

  // Mettre threadRunning à true AVANT de créer le thread pour éviter les race conditions
  threadRunning = true;
  connected = true;

  BaseType_t result = xTaskCreatePinnedToCore(
    threadFunction,           // Fonction du thread
    "MqttTask",              // Nom du thread
    STACK_SIZE_MQTT,         // Taille de la stack
    nullptr,                 // Paramètre
    PRIORITY_MQTT,           // Priorité
    &taskHandle,             // Handle
    CORE_MQTT                // Core
  );

  if (result != pdPASS) {
    LogManager::error("[MQTT] Erreur creation thread");
    threadRunning = false;
    connected = false;
    taskHandle = nullptr;
    return false;
  }

  #if ENABLE_VERBOSE_LOGS
  LogManager::info("[MQTT] Thread demarré!");
  #endif

  // Attendre un peu pour que le thread démarre
  vTaskDelay(pdMS_TO_TICKS(100));

  // Publier le statut "online"
  publishStatus();

  return true;
}

void MqttManager::disconnect() {
  if (!initialized) {
    return;
  }

  // Arrêter le thread : il s'auto-supprime via vTaskDelete(nullptr) quand il voit threadRunning=false
  if (taskHandle != nullptr) {
    threadRunning = false;
    vTaskDelay(pdMS_TO_TICKS(150));  // Laisser le temps au thread de sortir et s'auto-supprimer
    taskHandle = nullptr;
  }

  connected = false;
  LogManager::info("[MQTT] Deconnecte");
}

void MqttManager::shutdownForOta() {
  if (!initialized) {
    return;
  }
  // Arrêter le thread
  if (taskHandle != nullptr) {
    threadRunning = false;
    vTaskDelay(pdMS_TO_TICKS(150));
    taskHandle = nullptr;
  }
  // Libérer la queue (libère RAM)
  if (publishQueue != nullptr) {
    vQueueDelete(publishQueue);
    publishQueue = nullptr;
  }
  connected = false;
  initialized = false;
  LogManager::info("[MQTT] shutdownForOta: task+queue liberes");
}

bool MqttManager::isConnected() {
  return initialized && connected && threadRunning && WiFiManager::isConnected() && mqttClient.connected();
}

bool MqttManager::isInitialized() {
  return initialized;
}

bool MqttManager::isAvailable() {
  return initialized && WiFiManager::isConnected();
}

void MqttManager::loop() {
  // Le thread gère tout, cette fonction ne fait rien
  // Gardée pour compatibilité avec le code existant
}

void MqttManager::threadFunction(void* parameter) {
  LogManager::info("[MQTT] Thread actif - entrée dans threadFunction");

  int loopCount = 0;
  while (threadRunning) {
    loopCount++;

    // Log périodique pour vérifier que la boucle tourne
    if (loopCount == 1) {
      LogManager::info("[MQTT] Première itération de la boucle");
    } else if (loopCount % 500 == 0) {
      LogManager::debug("[MQTT] Boucle active (iteration %d)", loopCount);
    }

    // Vérifier la connexion WiFi
    if (!WiFiManager::isConnected()) {
      if (connected) {
        connected = false;
        LogManager::warning("[MQTT] WiFi perdu");
      }
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    // Reconnecter si nécessaire
    if (!mqttClient.connected()) {
      if (mqttClient.connect(clientId, mqttUsername, mqttPassword)) {
        mqttClient.subscribe(cmdTopic);
        connected = true;
        publishStatus();
        LogManager::info("[MQTT] Connecté au broker %s:%d", mqttBrokerHost, mqttBrokerPort);
      } else {
        connected = false;
        LogManager::warning("[MQTT] Echec connexion au broker");
        vTaskDelay(pdMS_TO_TICKS(5000));
        continue;
      }
    }

    // Traiter les messages à publier en attente
    if (publishQueue != nullptr) {
      PublishMessage pubMsg;
      while (xQueueReceive(publishQueue, &pubMsg, 0) == pdTRUE) {
        publishInternal(pubMsg.message);
      }
    }

    // Traiter les messages reçus et gérer keep-alive TCP
    mqttClient.loop();

    // Petit délai entre les itérations
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  LogManager::debug("[MQTT] Thread arrête (threadRunning=false)");
  vTaskDelete(nullptr);
}

void MqttManager::onMessage(char* topic, byte* payload, unsigned int length) {
  // Copier le payload dans un buffer null-terminated
  char buffer[1024];
  unsigned int copyLen = min(length, (unsigned int)sizeof(buffer) - 1);
  memcpy(buffer, payload, copyLen);
  buffer[copyLen] = '\0';

  LogManager::debug("[MQTT] Message reçu sur topic: %s", topic);

  // Parser le JSON
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, buffer);
  if (error) {
    LogManager::error("[MQTT] Erreur parsing JSON: %s", error.c_str());
    return;
  }

  JsonObject obj = doc.as<JsonObject>();
  if (obj.isNull()) {
    LogManager::error("[MQTT] JSON n'est pas un object");
    return;
  }

  // Ignorer les messages de status/response pour éviter de retraiter les propres publications
  if (obj["status"].is<const char*>() || obj["response"].is<const char*>()) {
    LogManager::debug("[MQTT] Message ignoré (status/response)");
    return;
  }

  // Traiter le message via les routes spécifiques au modèle
  ModelMqttRoutes::processMessage(obj);
}

bool MqttManager::publishInternal(const char* message) {
  if (!mqttClient.connected()) {
    LogManager::warning("[MQTT] Client non connecté, impossible de publier");
    return false;
  }
  return mqttClient.publish(telemetryTopic, message);
}

bool MqttManager::publish(const char* message) {
  if (!initialized) {
    LogManager::warning("[MQTT] Manager non initialisé");
    return false;
  }

  if (publishQueue == nullptr) {
    LogManager::error("[MQTT] Queue de publication non initialisée");
    return false;
  }

  // Mettre le message en queue (thread-safe)
  PublishMessage pubMsg;
  strncpy(pubMsg.message, message, sizeof(pubMsg.message) - 1);
  pubMsg.message[sizeof(pubMsg.message) - 1] = '\0';

  if (xQueueSend(publishQueue, &pubMsg, pdMS_TO_TICKS(100)) != pdTRUE) {
    LogManager::warning("[MQTT] Queue pleine, message perdu");
    return false;
  }

  return true;
}

bool MqttManager::publishStatus() {
  if (!isInitialized()) return false;

  JsonDocument doc;
  doc["status"] = "online";
  doc["device"] = "Kidoo";
  doc["timestamp"] = millis();

  String payload;
  serializeJson(doc, payload);

  return publish(payload.c_str());
}

const char* MqttManager::getCmdTopic() {
  return cmdTopic;
}

const char* MqttManager::getTelemetryTopic() {
  return telemetryTopic;
}

void MqttManager::printInfo() {
  Serial.println("\n=== MQTT Manager Info ===");
  Serial.printf("Initialized : %d\n", initialized);
  Serial.printf("Connected   : %d\n", connected);
  Serial.printf("Thread Running : %d\n", threadRunning);
  Serial.printf("Cmd Topic   : %s\n", cmdTopic);
  Serial.printf("Telemetry Topic : %s\n", telemetryTopic);
  Serial.printf("Client ID   : %s\n", clientId);
  Serial.printf("Broker      : %s:%d\n", mqttBrokerHost, mqttBrokerPort);
  Serial.printf("MQTT Connected : %d\n", mqttClient.connected());
  Serial.println("========================\n");
}

#endif // HAS_MQTT
