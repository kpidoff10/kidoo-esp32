#include "ble_manager.h"
#include "../../../model_config.h"
#include "commands/ble_command_handler.h"
#include "../ble_config/ble_config_manager.h"
#include "../../config/core_config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#ifdef HAS_BLE
#include <string>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// UUIDs pour le service et les caractéristiques BLE
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_RX "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_UUID_TX "beb5483e-36e1-4688-b7f5-ea07361b26a9"

// Taille maximale d'une commande BLE (base64 décodé peut être jusqu'à 512 bytes)
#define BLE_COMMAND_MAX_SIZE 512

// Structure pour passer les commandes BLE à la tâche de traitement
struct BLECommandMessage {
  char data[BLE_COMMAND_MAX_SIZE + 1];  // +1 pour le null terminator
  size_t length;
};

// Variables statiques BLE (seulement si HAS_BLE est défini)
static BLEServer* pServer = nullptr;
static BLEService* pService = nullptr;
static BLECharacteristic* pTxCharacteristic = nullptr;
static QueueHandle_t bleCommandQueue = nullptr;
static TaskHandle_t bleCommandTaskHandle = nullptr;
static bool commandTaskRunning = false;

// Tâche FreeRTOS pour traiter les commandes BLE avec une stack plus grande
void bleCommandTask(void* parameter) {
  BLECommandMessage msg;
  
  Serial.println("[BLE-TASK] Tâche de traitement des commandes BLE démarrée");
  
  while (commandTaskRunning && bleCommandQueue != nullptr) {
    // Attendre une commande dans la queue (timeout de 1 seconde)
    if (xQueueReceive(bleCommandQueue, &msg, pdMS_TO_TICKS(1000)) == pdTRUE) {
      // Nettoyer les données : supprimer les caractères nuls et espaces en fin
      String data = String(msg.data);
      data.trim();
      
      // Supprimer les caractères nuls éventuels
      int nullPos = data.indexOf('\0');
      if (nullPos >= 0) {
        data = data.substring(0, nullPos);
      }
      
      if (data.length() > 0) {
        Serial.println("[BLE-TASK] ========================================");
        Serial.println("[BLE-TASK] >>> COMMANDE BLE RECUE <<<");
        Serial.print("[BLE-TASK] Taille des donnees: ");
        Serial.println(data.length());
        Serial.print("[BLE-TASK] Donnees brutes: ");
        Serial.println(data);
        
        // Traiter la commande (avec une stack plus grande)
        Serial.println("[BLE-TASK] Appel de BLECommandHandler::handleCommand...");
        bool result = BLECommandHandler::handleCommand(data);
        Serial.print("[BLE-TASK] Resultat de handleCommand: ");
        Serial.println(result ? "true" : "false");
        
        Serial.println("[BLE-TASK] ========================================");
      }
    }
  }
  
  Serial.println("[BLE-TASK] Tâche de traitement des commandes BLE arrêtée");
  vTaskDelete(nullptr);
}

// Callback pour les données reçues sur la caractéristique RX
// Ce callback doit être léger et rapide pour éviter les débordements de stack
class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    // Vérifier que la queue existe
    if (bleCommandQueue == nullptr) {
      Serial.println("[BLE] ERREUR: Queue de commandes BLE non initialisée");
      return;
    }
    
    // getValue() retourne std::string (BLE library)
    std::string value = pCharacteristic->getValue();
    if (value.empty()) {
      return;
    }
    
    // Limiter la taille pour éviter les débordements
    if (value.length() > BLE_COMMAND_MAX_SIZE) {
      Serial.print("[BLE] ERREUR: Commande trop longue (");
      Serial.print(value.length());
      Serial.print(" > ");
      Serial.print(BLE_COMMAND_MAX_SIZE);
      Serial.println(")");
      return;
    }
    
    // Préparer le message pour la queue
    BLECommandMessage msg;
    strncpy(msg.data, value.c_str(), BLE_COMMAND_MAX_SIZE);
    msg.data[BLE_COMMAND_MAX_SIZE] = '\0';  // Assurer le null terminator
    msg.length = value.length();
    
    // Envoyer la commande à la queue (non-bloquant avec timeout de 0)
    // Si la queue est pleine, on ignore la commande (ne devrait pas arriver)
    if (xQueueSend(bleCommandQueue, &msg, 0) != pdTRUE) {
      Serial.println("[BLE] ERREUR: Impossible d'envoyer la commande à la queue (pleine?)");
    } else {
      Serial.println("[BLE] Commande envoyée à la queue de traitement");
    }
  }
};

// Callbacks pour les événements de connexion/déconnexion
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    Serial.println("[BLE] ========================================");
    Serial.println("[BLE] >>> CONNEXION BLE ETABLIE <<<");
    Serial.print("[BLE] ID de connexion: ");
    Serial.println(pServer->getConnId());
    Serial.print("[BLE] Nombre de clients connectes: ");
    Serial.println(pServer->getConnectedCount());
    Serial.print("[BLE] Nom du dispositif: ");
    const char* deviceName = BLEManager::getDeviceName();
    if (deviceName != nullptr) {
      Serial.println(deviceName);
    } else {
      Serial.println("N/A");
    }
    Serial.println("[BLE] MTU configure a 512 bytes");
    Serial.println("[BLE] Le client peut maintenant envoyer des commandes");
    Serial.println("[BLE] ========================================");
  }

  void onDisconnect(BLEServer* pServer) {
    Serial.println("[BLE] ========================================");
    Serial.println("[BLE] >>> DECONNEXION BLE <<<");
    Serial.print("[BLE] Nombre de clients restants: ");
    Serial.println(pServer->getConnectedCount());
    Serial.println("[BLE] ========================================");
    
    // Redémarrer l'advertising si le BLE est toujours activé (via BLEConfigManager)
    // Cela permet de rediffuser immédiatement après une déconnexion
    #ifdef HAS_BLE
    if (BLEConfigManager::isInitialized() && BLEConfigManager::isBLEEnabled()) {
      // Attendre un court délai pour s'assurer que la déconnexion est complète
      delay(100);
      // Redémarrer l'advertising pour permettre une nouvelle connexion
      BLEManager::startAdvertising();
      Serial.println("[BLE] Advertising redemarre apres deconnexion (BLE toujours active)");
    }
    #endif
  }
};
#endif

// Variables statiques communes
bool BLEManager::initialized = false;
bool BLEManager::available = false;
char* BLEManager::deviceName = nullptr;

bool BLEManager::init(const char* deviceName) {
  // Si déjà initialisé, nettoyer d'abord les ressources existantes
  if (initialized) {
    // Arrêter la tâche de traitement des commandes si elle existe
    if (commandTaskRunning && bleCommandTaskHandle != nullptr) {
      commandTaskRunning = false;
      // Attendre un peu pour que la tâche termine sa boucle
      vTaskDelay(pdMS_TO_TICKS(100));
      // Supprimer la tâche
      if (bleCommandTaskHandle != nullptr) {
        vTaskDelete(bleCommandTaskHandle);
        bleCommandTaskHandle = nullptr;
      }
    }
    
    // Supprimer la queue si elle existe
    if (bleCommandQueue != nullptr) {
      vQueueDelete(bleCommandQueue);
      bleCommandQueue = nullptr;
    }
  }
  
  initialized = true;
  available = false;
  
#ifndef HAS_BLE
  // BLE non disponible sur ce modèle
  Serial.println("[BLE] BLE non disponible sur ce modèle");
  return false;
#else
  // BLE disponible, initialiser
  Serial.println("[BLE] Initialisation du BLE...");
  Serial.print("[BLE] Nom du dispositif: ");
  Serial.println(deviceName);
  
  // Allouer et copier le nom du device
  if (BLEManager::deviceName != nullptr) {
    free(BLEManager::deviceName);
  }
  BLEManager::deviceName = (char*)malloc(strlen(deviceName) + 1);
  if (BLEManager::deviceName == nullptr) {
    Serial.println("[BLE] ERREUR: Impossible d'allouer la memoire pour le nom");
    return false;
  }
  strcpy(BLEManager::deviceName, deviceName);
  
  // Initialiser BLEDevice
  BLEDevice::init(deviceName);
  
  // Configurer le MTU pour permettre l'envoi de commandes plus longues
  // MTU de 512 bytes (maximum recommandé pour BLE)
  BLEDevice::setMTU(512);
  Serial.println("[BLE] MTU configure a 512 bytes");
  
  // Créer le serveur BLE
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  // Créer le service
  pService = pServer->createService(SERVICE_UUID);
  
  // Créer la caractéristique TX (notify)
  pTxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID_TX,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  // Note: BLE2902 est déprécié dans NimBLE mais nécessaire pour la compatibilité
  // Le descripteur est automatiquement ajouté par NimBLE pour les notifications
  #ifndef CONFIG_BT_NIMBLE_ENABLED
  pTxCharacteristic->addDescriptor(new BLE2902());
  #endif
  
  // Créer la caractéristique RX (write)
  BLECharacteristic* pRxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID_RX,
    BLECharacteristic::PROPERTY_WRITE
  );
  pRxCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  
  // Initialiser le command handler avec la caractéristique TX
  BLECommandHandler::init(pTxCharacteristic);
  
  // Créer la queue pour les commandes BLE (taille de 5 commandes max en attente)
  bleCommandQueue = xQueueCreate(5, sizeof(BLECommandMessage));
  if (bleCommandQueue == nullptr) {
    Serial.println("[BLE] ERREUR: Impossible de créer la queue de commandes BLE");
    available = false;
    return false;
  }
  
  // Créer la tâche FreeRTOS pour traiter les commandes BLE
  commandTaskRunning = true;
  BaseType_t taskResult = xTaskCreatePinnedToCore(
    bleCommandTask,              // Fonction de la tâche
    "BLECommandTask",           // Nom de la tâche
    STACK_SIZE_BLE_COMMAND,     // Taille de la stack (définie dans core_config.h)
    nullptr,                     // Paramètres
    PRIORITY_BLE_COMMAND,       // Priorité (définie dans core_config.h)
    &bleCommandTaskHandle,      // Handle
    CORE_BLE                    // Core (défini dans core_config.h)
  );
  
  if (taskResult != pdPASS) {
    Serial.println("[BLE] ERREUR: Impossible de créer la tâche de traitement des commandes BLE");
    vQueueDelete(bleCommandQueue);
    bleCommandQueue = nullptr;
    commandTaskRunning = false;
    available = false;
    return false;
  }
  
  Serial.println("[BLE] Queue et tâche de traitement des commandes BLE créées");
  
  // Démarrer le service
  pService->start();
  
  // Configurer l'advertising (mais ne PAS le démarrer automatiquement)
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x0C);
  
  // IMPORTANT: Ne PAS démarrer l'advertising ici
  // L'advertising sera démarré uniquement via BLEConfigManager::enableBLE()
  // ou via BLEManager::startAdvertising() explicitement
  
  available = true;
  
  Serial.println("[BLE] ========================================");
  Serial.println("[BLE] BLE initialise avec succes !");
  Serial.print("[BLE] Nom du dispositif: ");
  Serial.println(deviceName);
  Serial.print("[BLE] Service UUID: ");
  Serial.println(SERVICE_UUID);
  Serial.println("[BLE] Advertising desactive par defaut");
  Serial.println("[BLE] Le BLE sera active via appui long sur bouton ou automatiquement si WiFi non connecte");
  Serial.println("[BLE] ========================================");
  
  return true;
#endif
}

bool BLEManager::isAvailable() {
  return initialized && available;
}

bool BLEManager::isInitialized() {
  return initialized;
}

void BLEManager::startAdvertising() {
#ifdef HAS_BLE
  if (!available || pServer == nullptr) {
    Serial.println("[BLE] ERREUR: Impossible de demarrer l'advertising (BLE non initialise)");
    return;
  }
  
  // Démarrer l'advertising (double démarrage pour s'assurer qu'il démarre bien)
  BLEDevice::startAdvertising();
  delay(200);
  BLEDevice::startAdvertising();
  Serial.println("[BLE] Advertising demarre");
  Serial.println("[BLE] Le dispositif est maintenant visible en Bluetooth");
#endif
}

void BLEManager::stopAdvertising() {
#ifdef HAS_BLE
  if (!available || pServer == nullptr) {
    return;
  }
  
  BLEDevice::stopAdvertising();
  Serial.println("[BLE] Advertising arrete");
#endif
}

bool BLEManager::isConnected() {
#ifdef HAS_BLE
  if (!available || pServer == nullptr) {
    return false;
  }
  
  // Vérifier si le serveur a des clients connectés
  return pServer->getConnectedCount() > 0;
#else
  return false;
#endif
}

const char* BLEManager::getDeviceName() {
  return deviceName;
}