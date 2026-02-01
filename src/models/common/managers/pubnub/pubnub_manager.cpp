#include "pubnub_manager.h"
#include "../../../model_config.h"
#include "../../config/core_config.h"

#ifdef HAS_PUBNUB

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_mac.h>  // Pour esp_read_mac() et ESP_MAC_WIFI_STA
#include "../wifi/wifi_manager.h"
#include "../serial/serial_commands.h"
#include "../init/init_manager.h"
#include "../../../model_pubnub_routes.h"

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
  
  Serial.println("[PUBNUB] Initialisation...");
  
  // Vérifier que les clés sont configurées
  if (strlen(DEFAULT_PUBNUB_SUBSCRIBE_KEY) == 0) {
    Serial.println("[PUBNUB] Subscribe key non configuree dans default_config.h");
    return false;
  }
  
  // Construire le nom du channel basé sur l'adresse MAC WiFi (unique par appareil)
  // Sur ESP32-C3, BLE et WiFi ont des adresses MAC différentes
  // On obtient directement les bytes de la MAC pour construire le channel
  uint8_t mac[6];
  esp_err_t err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
  if (err != ESP_OK) {
    // Fallback: utiliser WiFi.macAddress() si esp_read_mac() échoue
    Serial.print("[PUBNUB] esp_read_mac() échoué (err=");
    Serial.print(err);
    Serial.println("), utilisation de WiFi.macAddress()");
    WiFi.macAddress(mac);
  }
  snprintf(channel, sizeof(channel), "kidoo-%02X%02X%02X%02X%02X%02X",
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.print("[PUBNUB] Channel construit avec MAC: ");
  Serial.println(channel);
  
  // Créer la file d'attente pour les publications
  publishQueue = xQueueCreate(PUBLISH_QUEUE_SIZE, sizeof(PublishMessage));
  if (publishQueue == nullptr) {
    Serial.println("[PUBNUB] Erreur creation queue");
    return false;
  }
  
  initialized = true;
  Serial.println("[PUBNUB] Initialisation OK");
  Serial.print("[PUBNUB] Channel: ");
  Serial.println(channel);
  
  return true;
}

bool PubNubManager::connect() {
  Serial.print("[PUBNUB] connect() appelé - initialized: ");
  Serial.print(initialized);
  Serial.print(", threadRunning: ");
  Serial.print(threadRunning);
  Serial.print(", WiFi connecté: ");
  Serial.println(WiFiManager::isConnected());
  
  if (!initialized) {
    Serial.println("[PUBNUB] Non initialise");
    return false;
  }
  
  // Vérifier que le WiFi est connecté
  if (!WiFiManager::isConnected()) {
    Serial.println("[PUBNUB] WiFi non connecte");
    return false;
  }
  
  // Si le thread tourne déjà, on est déjà connecté
  if (threadRunning) {
    Serial.println("[PUBNUB] Deja connecte (threadRunning=true)");
    return true;
  }
  
  // Si taskHandle existe mais threadRunning est false, nettoyer d'abord
  if (taskHandle != nullptr) {
    Serial.println("[PUBNUB] Nettoyage d'un ancien thread...");
    vTaskDelete(taskHandle);
    taskHandle = nullptr;
  }
  
  Serial.println("[PUBNUB] Demarrage du thread...");
  
  // Reset le timetoken pour commencer fresh
  strcpy(timeToken, "0");
  
  // Créer le thread FreeRTOS sur Core 0 (même core que WiFi stack)
  Serial.printf("[PUBNUB] Core=%d, Priority=%d, Stack=%d\n", TASK_CORE, TASK_PRIORITY, STACK_SIZE);
  
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
  
  Serial.println("[PUBNUB] Thread demarre!");
  
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
  
  // Arrêter le thread
  if (taskHandle != nullptr) {
    threadRunning = false;
    vTaskDelay(pdMS_TO_TICKS(100)); // Laisser le temps au thread de s'arrêter
    vTaskDelete(taskHandle);
    taskHandle = nullptr;
  }
  
  connected = false;
  strcpy(timeToken, "0");
  Serial.println("[PUBNUB] Deconnecte");
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
  Serial.println("[PUBNUB] Thread actif - entrée dans threadFunction");
  
  int loopCount = 0;
  while (threadRunning) {
    loopCount++;
    
    // Log périodique pour vérifier que la boucle tourne (toutes les 500 itérations = ~50 secondes)
    if (loopCount == 1) {
      Serial.println("[PUBNUB] Première itération de la boucle");
    } else if (loopCount % 500 == 0) {
      Serial.print("[PUBNUB] Boucle active (iteration ");
      Serial.print(loopCount);
      Serial.println(")");
    }
    
    // Vérifier la connexion WiFi
    if (!WiFiManager::isConnected()) {
      if (connected) {
        connected = false;
        Serial.println("[PUBNUB] WiFi perdu");
      }
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }
    
    // Reconnecter si nécessaire
    if (!connected) {
      connected = true;
      strcpy(timeToken, "0");
      Serial.println("[PUBNUB] WiFi retrouve, reconnexion...");
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
  
  Serial.println("[PUBNUB] Thread arrete (threadRunning=false)");
  vTaskDelete(nullptr);
}

bool PubNubManager::subscribe() {
  if (!WiFiManager::isConnected()) {
    Serial.println("[PUBNUB] Subscribe: WiFi non connecté");
    return false;
  }
  
  if (strlen(channel) == 0) {
    Serial.println("[PUBNUB] Subscribe: Channel vide!");
    return false;
  }
  
  if (strlen(DEFAULT_PUBNUB_SUBSCRIBE_KEY) == 0) {
    Serial.println("[PUBNUB] Subscribe: Subscribe key vide!");
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
      Serial.print("[PUBNUB] Réponse reçue (");
      Serial.print(payload.length());
      Serial.println(" bytes)");
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
      Serial.print("[PUBNUB] Erreur subscribe HTTP: ");
      Serial.println(httpCode);
    }
    http.end();
    return false;
  }
}

void PubNubManager::processMessages(const char* json) {
  // Log pour debug : afficher le JSON brut reçu
  if (json != nullptr && strlen(json) > 0) {
    Serial.print("[PUBNUB] JSON brut reçu: ");
    // Limiter la taille du log pour éviter les problèmes de mémoire
    int len = strlen(json);
    if (len > 200) {
      Serial.print("(tronqué, ");
      Serial.print(len);
      Serial.print(" bytes) ");
      char truncated[201];
      strncpy(truncated, json, 200);
      truncated[200] = '\0';
      Serial.println(truncated);
    } else {
      Serial.println(json);
    }
  }
  
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, json);
  
  if (error) {
    Serial.print("[PUBNUB] Erreur parsing JSON: ");
    Serial.println(error.c_str());
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
    Serial.print("[PUBNUB] ");
    Serial.print(messageCount);
    Serial.println(" message(s) reçu(s)");
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
          Serial.println("[PUBNUB] WARNING: Format de message incorrect detecte (action est un objet)");
        }
      }
      
      if (action != nullptr) {
        Serial.print("[PUBNUB] Commande reçue - Action: ");
        Serial.print(action);
        
        // Log des paramètres si présents (de manière sûre)
        if (actionObj["params"].is<JsonObject>()) {
          Serial.print(" (avec params)");
        } else if (actionObj["value"].is<int>()) {
          Serial.print(" - value: ");
          Serial.print(actionObj["value"].as<int>());
        } else if (actionObj["value"].is<float>()) {
          Serial.print(" - value: ");
          Serial.print(actionObj["value"].as<float>());
        } else if (actionObj["delay"].is<int>()) {
          Serial.print(" - delay: ");
          Serial.print(actionObj["delay"].as<int>());
          Serial.print("ms");
        }
        
        // Log du timestamp si présent
        if (actionObj["timestamp"].is<unsigned long>() || actionObj["timestamp"].is<long>()) {
          Serial.print(" - timestamp: ");
          Serial.print(actionObj["timestamp"].as<unsigned long>());
        }
        Serial.println();
        
        ModelPubNubRoutes::processMessage(actionObj);
      }
      // Si c'est une commande série (legacy)
      else if (obj["cmd"].is<const char*>()) {
        const char* cmd = obj["cmd"].as<const char*>();
        if (cmd != nullptr) {
          Serial.print("[PUBNUB] Commande série (legacy) reçue: ");
          Serial.println(cmd);
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
  
  Serial.print("[PUBNUB] Commande recue: ");
  Serial.println(command);
  
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

bool PubNubManager::publishInternal(const char* message) {
  if (!WiFiManager::isConnected()) {
    return false;
  }
  
  if (strlen(DEFAULT_PUBNUB_PUBLISH_KEY) == 0) {
    return false;
  }
  
  HTTPClient http;
  
  // Préparer le message JSON
  String jsonMessage;
  if (message[0] == '{' || message[0] == '[') {
    jsonMessage = message;
  } else {
    jsonMessage = "\"";
    jsonMessage += message;
    jsonMessage += "\"";
  }
  
  // Construire l'URL (sans le message - on utilise POST)
  String url = "http://";
  url += PUBNUB_ORIGIN;
  url += "/publish/";
  url += DEFAULT_PUBNUB_PUBLISH_KEY;
  url += "/";
  url += DEFAULT_PUBNUB_SUBSCRIBE_KEY;
  url += "/0/";
  url += channel;
  url += "/0";
  
  http.begin(url);
  http.setTimeout(5000);
  http.addHeader("Content-Type", "application/json");
  
  // Utiliser POST pour envoyer le message dans le body (pas de limite URL)
  int httpCode = http.POST(jsonMessage);
  http.end();
  
  if (httpCode == HTTP_CODE_OK) {
    return true;
  } else {
    Serial.print("[PUBNUB] Erreur publish: ");
    Serial.println(httpCode);
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
  Serial.println("");
  Serial.println("========== Etat PubNub ==========");
  
  Serial.print("[PUBNUB] Initialise: ");
  Serial.println(initialized ? "Oui" : "Non");
  
  if (!initialized) {
    Serial.println("=================================");
    return;
  }
  
  Serial.print("[PUBNUB] Channel: ");
  Serial.println(channel);
  
  Serial.print("[PUBNUB] Thread actif: ");
  Serial.println(threadRunning ? "Oui" : "Non");
  
  Serial.print("[PUBNUB] Connecte: ");
  Serial.println(connected ? "Oui" : "Non");
  
  Serial.print("[PUBNUB] TimeToken: ");
  Serial.println(timeToken);
  
  // Afficher la mémoire libre du thread
  if (taskHandle != nullptr) {
    Serial.print("[PUBNUB] Stack libre: ");
    Serial.print(uxTaskGetStackHighWaterMark(taskHandle));
    Serial.println(" bytes");
  }
  
  Serial.println("=================================");
}

const char* PubNubManager::getChannel() {
  return channel;
}

#else // !HAS_PUBNUB

// Implémentation vide si PubNub n'est pas disponible
bool PubNubManager::init() { return false; }
bool PubNubManager::connect() { return false; }
void PubNubManager::disconnect() {}
bool PubNubManager::isConnected() { return false; }
bool PubNubManager::isInitialized() { return false; }
bool PubNubManager::isAvailable() { return false; }
void PubNubManager::loop() {}
bool PubNubManager::publish(const char*) { return false; }
bool PubNubManager::publishStatus() { return false; }
void PubNubManager::printInfo() {
  Serial.println("[PUBNUB] PubNub non disponible sur ce modele");
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
