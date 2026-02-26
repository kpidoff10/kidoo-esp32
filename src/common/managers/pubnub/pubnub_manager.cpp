#include "pubnub_manager.h"
#include "models/model_config.h"
#include "common/config/core_config.h"
#include "common/managers/log/log_manager.h"

#ifdef HAS_PUBNUB

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_mac.h>  // Pour esp_read_mac() et ESP_MAC_WIFI_STA
#include "common/managers/wifi/wifi_manager.h"
#include "common/managers/serial/serial_commands.h"
#include "common/managers/init/init_manager.h"
#include "models/model_pubnub_routes.h"

// Variables statiques
bool PubNubManager::initialized = false;
bool PubNubManager::connected = false;
bool PubNubManager::threadRunning = false;
char PubNubManager::channel[64] = "";
char PubNubManager::timeToken[32] = "0";
TaskHandle_t PubNubManager::taskHandle = nullptr;
QueueHandle_t PubNubManager::publishQueue = nullptr;

// URLs PubNub
static const char* PUBNUB_ORIGIN = "ps.pndsn.com";

// Structure pour les messages à publier
struct PublishMessage {
  char message[512];  // Augmenté pour supporter get-info
};

bool PubNubManager::init() {
  if (initialized) {
    return true;
  }
  
  // Vérifier que les clés sont configurées
  if (strlen(DEFAULT_PUBNUB_SUBSCRIBE_KEY) == 0) {
    LogManager::error("[PUBNUB] Subscribe key non configuree dans default_config.h");
    return false;
  }
  
  // Construire le nom du channel basé sur l'adresse MAC WiFi (unique par appareil)
  // Sur ESP32-C3, BLE et WiFi ont des adresses MAC différentes
  // On obtient directement les bytes de la MAC pour construire le channel
  uint8_t mac[6];
  esp_err_t err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
  if (err != ESP_OK) {
    // Fallback: utiliser WiFi.macAddress() si esp_read_mac() échoue
    LogManager::warning("[PUBNUB] esp_read_mac() échoué (err=%d), utilisation de WiFi.macAddress()", err);
    WiFi.macAddress(mac);
  }
  snprintf(channel, sizeof(channel), "kidoo-%02X%02X%02X%02X%02X%02X",
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  LogManager::info("[PUBNUB] Channel construit avec MAC: %s", channel);
  
  // Créer la file d'attente pour les publications
  publishQueue = xQueueCreate(PUBLISH_QUEUE_SIZE, sizeof(PublishMessage));
  if (publishQueue == nullptr) {
    LogManager::error("[PUBNUB] Erreur creation queue");
    return false;
  }
  
  initialized = true;
  
  return true;
}

bool PubNubManager::connect() {
  LogManager::debug("[PUBNUB] connect() appelé - initialized: %d, threadRunning: %d, WiFi: %d",
    initialized, threadRunning, WiFiManager::isConnected());
  
  if (!initialized) {
    LogManager::error("[PUBNUB] Non initialise");
    return false;
  }
  
  // Vérifier que le WiFi est connecté
  if (!WiFiManager::isConnected()) {
    LogManager::warning("[PUBNUB] WiFi non connecte");
    return false;
  }
  
  // Si le thread tourne déjà, on est déjà connecté
  if (threadRunning) {
    LogManager::debug("[PUBNUB] Deja connecte (threadRunning=true)");
    return true;
  }
  
  // Si taskHandle existe mais threadRunning est false, nettoyer d'abord
  if (taskHandle != nullptr) {
    LogManager::info("[PUBNUB] Nettoyage d'un ancien thread...");
    vTaskDelete(taskHandle);
    taskHandle = nullptr;
  }
  
  // Reset le timetoken pour commencer fresh
  strcpy(timeToken, "0");
  
  // Créer le thread FreeRTOS sur Core 0 (même core que WiFi stack)
  LogManager::debug("[PUBNUB] Core=%d, Priority=%d, Stack=%d", TASK_CORE, TASK_PRIORITY, STACK_SIZE);
  
  // IMPORTANT: Mettre threadRunning à true AVANT de créer le thread
  // pour éviter les conditions de course
  threadRunning = true;
  connected = true;
  
  BaseType_t result = xTaskCreatePinnedToCore(
    threadFunction,     // Fonction du thread
    "PubNubTask",       // Nom du thread
    STACK_SIZE,         // Taille de la stack
    nullptr,            // Paramètre
    TASK_PRIORITY,      // Priorité
    &taskHandle,        // Handle
    TASK_CORE           // Core 0 avec WiFi stack (configuré dans core_config.h)
  );
  
  if (result != pdPASS) {
    Serial.println("[PUBNUB] Erreur creation thread");
    threadRunning = false;
    connected = false;
    taskHandle = nullptr;
    return false;
  }
  
  #if ENABLE_VERBOSE_LOGS
  LogManager::info("[PUBNUB] Thread demarre!");
  #endif
  
  // Attendre un peu pour que le thread démarre
  vTaskDelay(pdMS_TO_TICKS(100));
  
  // Publier le statut "online"
  publishStatus();
  
  return true;
}

void PubNubManager::disconnect() {
  if (!initialized) {
    return;
  }
  
  // Arrêter le thread : il s'auto-supprime via vTaskDelete(nullptr) quand il voit threadRunning=false.
  // Ne PAS appeler vTaskDelete(taskHandle) ici : le handle devient invalide une fois le thread supprimé.
  if (taskHandle != nullptr) {
    threadRunning = false;
    vTaskDelay(pdMS_TO_TICKS(150));  // Laisser le temps au thread de sortir et s'auto-supprimer
    taskHandle = nullptr;
  }
  
  connected = false;
  strcpy(timeToken, "0");
  Serial.println("[PUBNUB] Deconnecte");
}

void PubNubManager::shutdownForOta() {
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
  strcpy(timeToken, "0");
  LogManager::info("[PUBNUB] shutdownForOta: task+queue liberes");
}

bool PubNubManager::isConnected() {
  return initialized && connected && threadRunning && WiFiManager::isConnected();
}

bool PubNubManager::isInitialized() {
  return initialized;
}

bool PubNubManager::isAvailable() {
  return initialized && 
         WiFiManager::isConnected() && 
         strlen(DEFAULT_PUBNUB_SUBSCRIBE_KEY) > 0;
}

void PubNubManager::loop() {
  // Le thread gère tout, cette fonction ne fait rien
  // Gardée pour compatibilité avec le code existant
}

void PubNubManager::threadFunction(void* parameter) {
  LogManager::debug("[PUBNUB] Thread actif - entrée dans threadFunction");
  
  int loopCount = 0;
  while (threadRunning) {
    loopCount++;
    
    // Log périodique pour vérifier que la boucle tourne (toutes les 500 itérations = ~50 secondes)
    if (loopCount == 1) {
      LogManager::debug("[PUBNUB] Première itération de la boucle");
    } else if (loopCount % 500 == 0) {
      LogManager::debug("[PUBNUB] Boucle active (iteration %d)", loopCount);
    }
    
    // Vérifier la connexion WiFi
    if (!WiFiManager::isConnected()) {
      if (connected) {
        connected = false;
        LogManager::warning("[PUBNUB] WiFi perdu");
      }
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }
    
    // Reconnecter si nécessaire
    if (!connected) {
      connected = true;
      strcpy(timeToken, "0");
    }
    
    // Traiter les messages à publier en attente
    if (publishQueue != nullptr) {
      PublishMessage pubMsg;
      while (xQueueReceive(publishQueue, &pubMsg, 0) == pdTRUE) {
        publishInternal(pubMsg.message);
      }
    }
    
    // Subscribe (long polling)
    bool subscribeResult = subscribe();
    if (!subscribeResult) {
      // Ne pas logger à chaque fois pour éviter le spam
      // Serial.println("[PUBNUB] Subscribe a échoué");
    }
    
    // Petit délai entre les polls
    vTaskDelay(pdMS_TO_TICKS(SUBSCRIBE_INTERVAL_MS));
  }
  
  LogManager::debug("[PUBNUB] Thread arrete (threadRunning=false)");
  vTaskDelete(nullptr);
}

bool PubNubManager::subscribe() {
  if (!WiFiManager::isConnected()) {
    LogManager::warning("[PUBNUB] Subscribe: WiFi non connecté");
    return false;
  }
  
  if (strlen(channel) == 0) {
    LogManager::error("[PUBNUB] Subscribe: Channel vide!");
    return false;
  }
  
  if (strlen(DEFAULT_PUBNUB_SUBSCRIBE_KEY) == 0) {
    LogManager::error("[PUBNUB] Subscribe: Subscribe key vide!");
    return false;
  }
  
  HTTPClient http;
  
  // Construire l'URL de subscribe
  char url[256];
  snprintf(url, sizeof(url),
    "http://%s/subscribe/%s/%s/0/%s",
    PUBNUB_ORIGIN,
    DEFAULT_PUBNUB_SUBSCRIBE_KEY,
    channel,
    timeToken
  );
  
  http.begin(url);
  http.setConnectTimeout(2000);
  http.setTimeout(5000); // 5 secondes - PubNub garde la connexion ouverte
  
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    http.end();
    
    // Log pour debug : afficher la taille de la réponse
    if (payload.length() > 0) {
      LogManager::debug("[PUBNUB] Réponse reçue (%zu bytes)", payload.length());
    }
    
    processMessages(payload.c_str());
    return true;
  } else {
    // Les erreurs -11 (HTTPC_ERROR_CONNECTION_LOST) et -1 (HTTPC_ERROR_CONNECTION_REFUSED/timeout)
    // sont normales avec PubNub en long polling
    // -11 : connexion fermée entre deux requêtes (normal)
    // -1 : timeout de connexion ou refus temporaire (peut arriver avec PubNub)
    // Ne logger que les autres erreurs pour éviter le spam
    if (httpCode != -11 && httpCode != -1) {
      LogManager::warning("[PUBNUB] Erreur subscribe HTTP: %d", httpCode);
    }
    http.end();
    return false;
  }
}

void PubNubManager::processMessages(const char* json) {
  // Log pour debug : afficher le JSON brut reçu
  if (json != nullptr && strlen(json) > 0) {
    int len = strlen(json);
    if (len > 200) {
      char truncated[201];
      strncpy(truncated, json, 200);
      truncated[200] = '\0';
      LogManager::debug("[PUBNUB] JSON brut reçu (tronqué, %d bytes): %s", len, truncated);
    } else {
      LogManager::debug("[PUBNUB] JSON brut reçu: %s", json);
    }
  }
  
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, json);
  
  if (error) {
    LogManager::error("[PUBNUB] Erreur parsing JSON: %s", error.c_str());
    return;
  }
  
  // Extraire le nouveau timetoken
  if (doc[1].is<const char*>()) {
    strncpy(timeToken, doc[1].as<const char*>(), sizeof(timeToken) - 1);
    timeToken[sizeof(timeToken) - 1] = '\0';
  }
  
  // Traiter les messages
  if (!doc[0].is<JsonArray>()) {
    return;
  }
  
  JsonArray messages = doc[0].as<JsonArray>();
  
  // Compter les messages de manière sûre
  int messageCount = 0;
  for (JsonVariant msg : messages) {
    messageCount++;
  }
  
  if (messageCount > 0) {
    LogManager::debug("[PUBNUB] %d message(s) reçu(s)", messageCount);
  }
  
  for (JsonVariant msg : messages) {
    if (msg.is<const char*>()) {
      // Message texte simple = commande série
      const char* textMsg = msg.as<const char*>();
      if (textMsg != nullptr) {
        Serial.print("[PUBNUB] Message texte reçu: ");
        Serial.println(textMsg);
        executeCommand(textMsg);
      }
    } else if (msg.is<JsonObject>()) {
      JsonObject obj = msg.as<JsonObject>();

      // Ignorer nos propres messages (status, response, type)
      if (obj["status"].is<const char*>() || obj["response"].is<const char*>() || obj["type"].is<const char*>()) {
        continue;
      }

      // Si c'est une action, utiliser les routes
      // Gérer le cas où action est une string ou un objet (format incorrect)
      JsonObject actionObj = obj;
      const char* action = nullptr;
      
      if (obj["action"].is<const char*>()) {
        // Format normal : action est une string
        action = obj["action"].as<const char*>();
      } else if (obj["action"].is<JsonObject>()) {
        // Format incorrect : action est un objet, extraire l'action depuis l'objet
        JsonObject nestedAction = obj["action"].as<JsonObject>();
        if (nestedAction["action"].is<const char*>()) {
          action = nestedAction["action"].as<const char*>();
          // Utiliser l'objet imbriqué comme message principal
          actionObj = nestedAction;
          LogManager::warning("[PUBNUB] Format de message incorrect detecte (action est un objet)");
        }
      }
      
      if (action != nullptr) {
        if (actionObj["params"].is<JsonObject>()) {
          LogManager::debug("[PUBNUB] Commande reçue - Action: %s (avec params)", action);
        } else if (actionObj["value"].is<int>()) {
          LogManager::debug("[PUBNUB] Commande reçue - Action: %s - value: %d", action, actionObj["value"].as<int>());
        } else if (actionObj["value"].is<float>()) {
          LogManager::debug("[PUBNUB] Commande reçue - Action: %s - value: %.2f", action, actionObj["value"].as<float>());
        } else if (actionObj["delay"].is<int>()) {
          LogManager::debug("[PUBNUB] Commande reçue - Action: %s - delay: %dms", action, actionObj["delay"].as<int>());
        } else if (actionObj["timestamp"].is<unsigned long>() || actionObj["timestamp"].is<long>()) {
          LogManager::debug("[PUBNUB] Commande reçue - Action: %s - timestamp: %lu", action, (unsigned long)actionObj["timestamp"].as<unsigned long>());
        } else {
          LogManager::debug("[PUBNUB] Commande reçue - Action: %s", action);
        }
        
        ModelPubNubRoutes::processMessage(actionObj);
      }
      // Si c'est une commande série (legacy)
      else if (obj["cmd"].is<const char*>()) {
        const char* cmd = obj["cmd"].as<const char*>();
        if (cmd != nullptr) {
          LogManager::debug("[PUBNUB] Commande série (legacy) reçue: %s", cmd);
          executeCommand(cmd);
        }
      }
      // Message JSON sans action reconnue - log minimal pour éviter les problèmes de mémoire
      else {
        Serial.println("[PUBNUB] Message JSON reçu (format non reconnu)");
      }
    }
  }
}

void PubNubManager::executeCommand(const char* command) {
  // Ignorer les messages vides
  if (command == nullptr || strlen(command) == 0) {
    return;
  }
  
  // Ignorer nos propres confirmations (évite la boucle infinie)
  // Les confirmations sont des JSON avec "response"
  if (strstr(command, "\"response\"") != nullptr) {
    return;
  }
  
  // Ignorer les messages de statut (évite la boucle infinie)
  if (strstr(command, "\"status\"") != nullptr) {
    return;
  }
  
  LogManager::debug("[PUBNUB] Commande recue: %s", command);
  
  // Exécuter via SerialCommands
  SerialCommands::processCommand(String(command));
  
  // Note: On ne publie plus de confirmation pour éviter les boucles
  // Si besoin, utiliser un channel séparé pour les réponses
}

bool PubNubManager::publish(const char* message) {
  if (!initialized || publishQueue == nullptr) {
    return false;
  }
  
  // Ajouter à la file d'attente (thread-safe)
  PublishMessage pubMsg;
  strncpy(pubMsg.message, message, sizeof(pubMsg.message) - 1);
  pubMsg.message[sizeof(pubMsg.message) - 1] = '\0';
  
  if (xQueueSend(publishQueue, &pubMsg, 0) != pdTRUE) {
    Serial.println("[PUBNUB] Queue pleine, message ignore");
    return false;
  }
  
  return true;
}

// Encode pour URL PubNub (identique au serveur encodeMessage)
static void urlEncodeMessage(const char* msg, String& out) {
  out.reserve(strlen(msg) * 3 + 1);
  out = "";
  for (const char* p = msg; *p != '\0'; p++) {
    unsigned char c = (unsigned char)*p;
    if (c == '"') out += "%22";
    else if (c == ' ') out += "%20";
    else if (c == '{') out += "%7B";
    else if (c == '}') out += "%7D";
    else if (c == ':') out += "%3A";
    else if (c == ',') out += "%2C";
    else if (c == '[') out += "%5B";
    else if (c == ']') out += "%5D";
    else if (c == '\\') out += "%5C";
    else if (c == '%') out += "%25";
    else if (c == '+') out += "%2B";
    else if (c >= 128) { char buf[4]; snprintf(buf, sizeof(buf), "%%%02X", c); out += buf; }
    else out += (char)c;
  }
}

bool PubNubManager::publishInternal(const char* message) {
  if (!WiFiManager::isConnected()) {
    return false;
  }
  
  if (strlen(DEFAULT_PUBNUB_PUBLISH_KEY) == 0) {
    return false;
  }
  
  // Même format que le serveur : GET avec payload URL-encodé (pas de wrapper {"message":"..."})
  String encoded;
  urlEncodeMessage(message, encoded);

  String url = "http://";
  url += PUBNUB_ORIGIN;
  url += "/publish/";
  url += DEFAULT_PUBNUB_PUBLISH_KEY;
  url += "/";
  url += DEFAULT_PUBNUB_SUBSCRIBE_KEY;
  url += "/0/";
  url += channel;
  url += "/0/";
  url += encoded;

  HTTPClient http;
  http.begin(url);
  http.setTimeout(5000);

  int httpCode = http.GET();
  String responseBody = http.getString();
  http.end();
  
  if (httpCode == HTTP_CODE_OK) {
    return true;
  } else {
    if (responseBody.length() > 0 && responseBody.length() < 128) {
      LogManager::warning("[PUBNUB] Erreur publish: %d - %s", httpCode, responseBody.c_str());
    } else {
      LogManager::warning("[PUBNUB] Erreur publish: %d", httpCode);
    }
    return false;
  }
}

bool PubNubManager::publishStatus() {
  char statusJson[128];
  snprintf(statusJson, sizeof(statusJson),
    "{\"status\":\"online\",\"device\":\"%s\",\"ip\":\"%s\"}",
    DEFAULT_DEVICE_NAME,
    WiFiManager::getLocalIP().c_str()
  );
  
  return publish(statusJson);
}

void PubNubManager::printInfo() {
  LogManager::info("");
  LogManager::info("========== Etat PubNub ==========");
  LogManager::info("[PUBNUB] Initialise: %s", initialized ? "Oui" : "Non");
  
  if (!initialized) {
    LogManager::info("=================================");
    return;
  }
  
  LogManager::info("[PUBNUB] Channel: %s", channel);
  LogManager::info("[PUBNUB] Thread actif: %s", threadRunning ? "Oui" : "Non");
  LogManager::info("[PUBNUB] Connecte: %s", connected ? "Oui" : "Non");
  LogManager::info("[PUBNUB] TimeToken: %s", timeToken);
  
  if (taskHandle != nullptr) {
    LogManager::info("[PUBNUB] Stack libre: %u bytes", uxTaskGetStackHighWaterMark(taskHandle));
  }
  
  LogManager::info("=================================");
}

const char* PubNubManager::getChannel() {
  return channel;
}

#else // !HAS_PUBNUB

// Implémentation vide si PubNub n'est pas disponible
bool PubNubManager::init() { return false; }
bool PubNubManager::connect() { return false; }
void PubNubManager::disconnect() {}
void PubNubManager::shutdownForOta() {}
bool PubNubManager::isConnected() { return false; }
bool PubNubManager::isInitialized() { return false; }
bool PubNubManager::isAvailable() { return false; }
void PubNubManager::loop() {}
bool PubNubManager::publish(const char*) { return false; }
bool PubNubManager::publishStatus() { return false; }
void PubNubManager::printInfo() {
  LogManager::info("[PUBNUB] PubNub non disponible sur ce modele");
}
const char* PubNubManager::getChannel() { return ""; }

// Variables statiques
bool PubNubManager::initialized = false;
bool PubNubManager::connected = false;
bool PubNubManager::threadRunning = false;
char PubNubManager::channel[64] = "";
char PubNubManager::timeToken[32] = "0";
TaskHandle_t PubNubManager::taskHandle = nullptr;
QueueHandle_t PubNubManager::publishQueue = nullptr;

#endif // HAS_PUBNUB
