#include "ble_manager.h"
#include "models/model_config.h"
#include "commands/ble_command_handler.h"
#include "common/managers/ble_config/ble_config_manager.h"
#include "common/managers/log/log_manager.h"
#include "common/config/core_config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#ifdef HAS_BLE
#include <string>
#include <cstring>
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
  
  LogManager::info("[BLE-TASK] Tâche de traitement des commandes BLE démarrée");
  
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
        LogManager::info("[BLE-TASK] ========================================");
        LogManager::info("[BLE-TASK] >>> COMMANDE BLE RECUE <<<");
        LogManager::info("[BLE-TASK] Taille des donnees: %u", (unsigned)data.length());
        LogManager::info("[BLE-TASK] Donnees brutes: %s", data.c_str());
        
        // Traiter la commande (avec une stack plus grande)
        LogManager::info("[BLE-TASK] Appel de BLECommandHandler::handleCommand...");
        bool result = BLECommandHandler::handleCommand(data);
        LogManager::info("[BLE-TASK] Resultat de handleCommand: %s", result ? "true" : "false");
        
        LogManager::info("[BLE-TASK] ========================================");
      }
    }
  }
  
  LogManager::info("[BLE-TASK] Tâche de traitement des commandes BLE arrêtée");
  vTaskDelete(nullptr);
}

// Callback pour les données reçues sur la caractéristique RX
// IMPORTANT: Ce callback s'exécute dans le contexte BTC_TASK (stack ~3KB sur ESP32-C3).
// Garder le code minimal : pas de gros buffers sur la stack, pas de LogManager (vsnprintf=512B).
class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    if (bleCommandQueue == nullptr) return;

    const std::string& value = pCharacteristic->getValue();
    size_t len = value.length();
    if (len == 0 || len > BLE_COMMAND_MAX_SIZE) return;

    // Buffer statique pour éviter 513 bytes sur la stack de BTC_TASK
    static BLECommandMessage msg;
    memcpy(msg.data, value.data(), len);
    msg.data[len] = '\0';
    msg.length = len;

    if (xQueueSend(bleCommandQueue, &msg, 0) != pdTRUE) {
      // Queue pleine - pas de log ici (trop lourd pour BTC_TASK)
    }
  }
};

// Callbacks pour les événements de connexion/déconnexion
// IMPORTANT: S'exécutent dans BTC_TASK - limiter les logs et le travail.
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    // Log minimal (1 seul appel) pour éviter stack overflow
    LogManager::info("[BLE] Connexion etablie (connId=%u)", pServer->getConnId());
  }

  void onDisconnect(BLEServer* pServer) {
    LogManager::info("[BLE] Deconnexion (restants=%d)", pServer->getConnectedCount());

    #ifdef HAS_BLE
    if (BLEConfigManager::isInitialized() && BLEConfigManager::isBLEEnabled()) {
      delay(100);
      BLEManager::startAdvertising();
    }
    #endif
  }
};
#endif

// Variables statiques communes
bool BLEManager::initialized = false;
bool BLEManager::available = false;
char* BLEManager::deviceName = nullptr;
const char* BLEManager::deviceNameForReinit = nullptr;

bool BLEManager::init(const char* deviceName) {
  // Conserver le pointeur pour ré-init après shutdown (ne pas libérer, ex: DEFAULT_DEVICE_NAME)
  deviceNameForReinit = deviceName;

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
  LogManager::info("[BLE] BLE non disponible sur ce modèle");
  return false;
#else
  // BLE disponible, initialiser
  LogManager::info("[BLE] Initialisation du BLE...");
  LogManager::info("[BLE] Nom du dispositif: %s", deviceName);
  
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
  LogManager::info("[BLE] MTU configure a 512 bytes");
  
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
    LogManager::error("[BLE] Impossible de créer la tâche de traitement des commandes BLE");
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
  
  LogManager::info("[BLE] ========================================");
  LogManager::info("[BLE] BLE initialise avec succes !");
  LogManager::info("[BLE] Nom du dispositif: %s", deviceName);
  LogManager::info("[BLE] Service UUID: %s", SERVICE_UUID);
  LogManager::info("[BLE] Advertising desactive par defaut");
  LogManager::info("[BLE] Le BLE sera active via appui long sur bouton ou automatiquement si WiFi non connecte");
  LogManager::info("[BLE] ========================================");
  
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
  LogManager::info("[BLE] Advertising demarre");
  LogManager::info("[BLE] Le dispositif est maintenant visible en Bluetooth");
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

void BLEManager::shutdownForOta() {
#ifndef HAS_BLE
  return;
#else
  if (!initialized) {
    return;
  }
  // Arrêter la tâche de commandes
  if (commandTaskRunning && bleCommandTaskHandle != nullptr) {
    commandTaskRunning = false;
    vTaskDelay(pdMS_TO_TICKS(150));
    bleCommandTaskHandle = nullptr;
  }
  // Supprimer la queue
  if (bleCommandQueue != nullptr) {
    vQueueDelete(bleCommandQueue);
    bleCommandQueue = nullptr;
  }
  // Désactiver l'advertising et libérer la stack BLE (BLEDevice::deinit)
  if (pServer != nullptr) {
    BLEDevice::stopAdvertising();
    BLEDevice::deinit(true);
    pServer = nullptr;
    pService = nullptr;
    pTxCharacteristic = nullptr;
  }
  // Libérer le nom du device
  if (deviceName != nullptr) {
    free(deviceName);
    deviceName = nullptr;
  }
  initialized = false;
  available = false;
  LogManager::info("[BLE] shutdownForOta: BLE completement desactive, mem liberee");
#endif
}

const char* BLEManager::getDeviceNameForReinit() {
  return deviceNameForReinit;
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