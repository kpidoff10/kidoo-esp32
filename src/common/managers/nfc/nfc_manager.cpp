#include "nfc_manager.h"
#include "models/model_config.h"
#include "common/config/core_config.h"
#include <Arduino.h>

#ifdef HAS_NFC
#include <Wire.h>
#include <Adafruit_PN532.h>
#include <string.h>

// Bus I2C : Wire1 si NFC_USE_WIRE1 defini (evite conflit avec Wire principal)
#ifdef NFC_USE_WIRE1
  #define NFC_WIRE Wire1
#else
  #define NFC_WIRE Wire
#endif

// Instance statique du PN532 (créée lors de l'initialisation)
static Adafruit_PN532* nfcInstance = nullptr;
#endif // HAS_NFC

// Variables statiques
bool NFCManager::initialized = false;
bool NFCManager::available = false;
uint32_t NFCManager::firmwareVersion = 0;
QueueHandle_t NFCManager::tagEventQueue = nullptr;

// Thread
TaskHandle_t NFCManager::taskHandle = nullptr;
SemaphoreHandle_t NFCManager::nfcMutex = nullptr;
volatile bool NFCManager::threadRunning = false;
volatile bool NFCManager::autoDetectEnabled = true;  // Scan auto toutes les 1s

// Dernier tag détecté
uint8_t NFCManager::lastUID[10] = {0};
uint8_t NFCManager::lastUIDLength = 0;
volatile bool NFCManager::tagPresent = false;
unsigned long NFCManager::lastDetectionTime = 0;

// Callback
NFCTagCallback NFCManager::tagCallback = nullptr;

// Configuration du thread NFC
#define NFC_TASK_STACK_SIZE 4096
#define NFC_TASK_PRIORITY 1           // Priorité minimale
#define NFC_SCAN_INTERVAL_MS 300      // Scan toutes les 300ms (bus dédié, zéro conflit)
#define NFC_SCAN_TIMEOUT_MS 50        // Timeout I2C confortable (bus dédié)
#define NFC_TAG_TIMEOUT_MS 2000       // Timeout pour considérer qu'un tag est parti

#ifdef HAS_NFC

// =========================
// Thread NFC — polling léger (20ms I2C / 1000ms)
// =========================
void NFCManager::nfcTask(void* parameter) {
  (void)parameter;

  Serial.printf("[NFC] Thread demarre sur Core %d (scan toutes les %dms)\n", xPortGetCoreID(), NFC_SCAN_INTERVAL_MS);
  threadRunning = true;

  uint8_t uid[10];
  uint8_t uidLength;

  while (true) {
    if (!autoDetectEnabled || nfcInstance == nullptr) {
      vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }

    // === Scan polling : 20ms I2C puis 1s de repos ===
    if (xSemaphoreTake(nfcMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      uint8_t success = nfcInstance->readPassiveTargetID(
        PN532_MIFARE_ISO14443A, uid, &uidLength, NFC_SCAN_TIMEOUT_MS);

      if (success) {
        if (uidLength > 10) uidLength = 10;
        memcpy(lastUID, uid, uidLength);
        lastUIDLength = uidLength;
        bool wasAbsent = !tagPresent;
        tagPresent = true;
        lastDetectionTime = millis();

        // Déclencher le callback quand le tag (ré)apparaît (absent → présent)
        if (wasAbsent && tagCallback != nullptr && tagEventQueue != nullptr) {
          Serial.printf("[NFC] Tag detecte! UID len=%d\n", uidLength);
          TagEvent ev = {};
          uint8_t len = (lastUIDLength > 10) ? 10 : lastUIDLength;
          memcpy(ev.uid, lastUID, len);
          ev.uidLength = len;
          ev.blockValid = false;
          xSemaphoreGive(nfcMutex);
          xQueueSend(tagEventQueue, &ev, 0);
        } else {
          xSemaphoreGive(nfcMutex);
        }
      } else {
        xSemaphoreGive(nfcMutex);
        if (tagPresent && (millis() - lastDetectionTime > NFC_TAG_TIMEOUT_MS)) {
          tagPresent = false;
          Serial.println("[NFC] Tag retire");
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(NFC_SCAN_INTERVAL_MS));
  }
}
#endif // HAS_NFC

// =========================
// API
// =========================
bool NFCManager::init() {
#ifdef HAS_NFC
  if (initialized) {
    return available;
  }
  
  initialized = true;
  available = false;
  firmwareVersion = 0;
  
  Serial.println("[NFC] Initialisation du gestionnaire NFC...");
  
  // Créer le mutex
  nfcMutex = xSemaphoreCreateMutex();
  if (!nfcMutex) {
    Serial.println("[NFC] ERREUR: Impossible de creer le mutex");
    return false;
  }

  // Créer la file d'événements tag (traitement reporté dans la loop principale)
  tagEventQueue = xQueueCreate(TAG_EVENT_QUEUE_LEN, sizeof(TagEvent));
  if (!tagEventQueue) {
    Serial.println("[NFC] ERREUR: Impossible de creer la file evenements tag");
    return false;
  }
  
  // Tester le hardware NFC
  available = testHardware();
  
  if (available) {
    // Créer le thread de détection
    BaseType_t result = xTaskCreatePinnedToCore(
      nfcTask,
      "NFCTask",
      NFC_TASK_STACK_SIZE,
      nullptr,
      NFC_TASK_PRIORITY,
      &taskHandle,
      0  // Core 0 (avec WiFi/BLE, pas avec l'audio sur Core 1)
    );
    
    if (result != pdPASS) {
      Serial.println("[NFC] ERREUR: Impossible de creer le thread NFC");
      available = false;
      return false;
    }
    
    Serial.println("[NFC] Thread de detection demarre sur Core 0");
    Serial.println("[NFC] Detection automatique activee");
  } else {
    Serial.println("[NFC] Hardware non detecte");
  }
  
  return available;
#else
  // NFC non disponible sur ce modèle
  initialized = true;
  available = false;
  Serial.println("[NFC] NFC non disponible sur ce modele");
  return false;
#endif // HAS_NFC
}

bool NFCManager::isAvailable() {
#ifdef HAS_NFC
  return initialized && available;
#else
  return false;
#endif // HAS_NFC
}

bool NFCManager::isInitialized() {
  return initialized;
}

uint32_t NFCManager::getFirmwareVersion() {
  return firmwareVersion;
}

void NFCManager::setTagCallback(NFCTagCallback callback) {
  tagCallback = callback;
  Serial.println("[NFC] Callback configure");
}

void NFCManager::processTagEvents() {
#ifdef HAS_NFC
  if (tagCallback == nullptr || tagEventQueue == nullptr) {
    return;
  }
  TagEvent ev;
  while (xQueueReceive(tagEventQueue, &ev, 0) == pdTRUE) {
    tagCallback(ev.uid, ev.uidLength, ev.blockData, ev.blockValid);
  }
#endif // HAS_NFC
}

void NFCManager::setAutoDetect(bool enabled) {
  autoDetectEnabled = enabled;
  if (enabled) {
    Serial.println("[NFC] Detection automatique activee");
  } else {
    Serial.println("[NFC] Detection automatique desactivee");
  }
}

bool NFCManager::isAutoDetectEnabled() {
  return autoDetectEnabled;
}

bool NFCManager::isTagPresent() {
  // Un tag est considéré présent s'il a été détecté récemment
  if (!tagPresent) return false;
  
  // Vérifier le timeout
  if (millis() - lastDetectionTime > NFC_TAG_TIMEOUT_MS) {
    tagPresent = false;
    return false;
  }
  
  return true;
}

bool NFCManager::getLastTagUID(uint8_t* uid, uint8_t* uidLength) {
#ifdef HAS_NFC
  if (!tagPresent || lastUIDLength == 0) {
    return false;
  }
  
  if (xSemaphoreTake(nfcMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    memcpy(uid, lastUID, lastUIDLength);
    *uidLength = lastUIDLength;
    xSemaphoreGive(nfcMutex);
    return true;
  }
  
  return false;
#else
  return false;
#endif // HAS_NFC
}

bool NFCManager::testHardware() {
#ifdef HAS_NFC
  Serial.println("[NFC] Test hardware...");
  Serial.println("[NFC] Mode: I2C");

  // Initialiser le bus I2C (safe à appeler plusieurs fois, Arduino garde les mêmes pins)
  NFC_WIRE.begin(NFC_SDA_PIN, NFC_SCL_PIN);
  delay(100);

  Serial.printf("[NFC] Pins I2C: SDA=%d, SCL=%d\n", NFC_SDA_PIN, NFC_SCL_PIN);
  Serial.printf("[NFC] Adresse I2C: 0x%02X\n", NFC_I2C_ADDRESS);

  // Créer une instance temporaire du PN532 pour le test (pas d'IRQ)
  Adafruit_PN532 nfc(-1, -1, &NFC_WIRE);

  nfc.begin();
  delay(200);

  // Tenter de lire la version du firmware
  Serial.println("[NFC] Lecture version firmware...");
  uint32_t versiondata = nfc.getFirmwareVersion();

  if (!versiondata) {
    Serial.println("[NFC] Module PN532 non detecte");
    Serial.println("[NFC] Verifiez:");
    Serial.printf("[NFC]   - Branchement SDA/SCL (GPIO %d/%d)\n", NFC_SDA_PIN, NFC_SCL_PIN);
    Serial.println("[NFC]   - Alimentation 3.3V");
    Serial.printf("[NFC]   - Adresse I2C 0x%02X\n", NFC_I2C_ADDRESS);
    firmwareVersion = 0;
    return false;
  }

  firmwareVersion = versiondata;

  Serial.print("[NFC] Chip PN5");
  Serial.println((versiondata >> 24) & 0xFF, HEX);
  Serial.print("[NFC] Firmware: ");
  Serial.print((versiondata >> 16) & 0xFF, DEC);
  Serial.print(".");
  Serial.println((versiondata >> 8) & 0xFF, DEC);

  nfc.SAMConfig();

  // Créer l'instance statique pour les opérations futures
  if (nfcInstance == nullptr) {
    nfcInstance = new Adafruit_PN532(-1, -1, &NFC_WIRE);
    nfcInstance->begin();
    delay(100);
    nfcInstance->SAMConfig();
  }

  Serial.println("[NFC] Hardware OK");
  return true;
#else
  return false;
#endif // HAS_NFC
}

bool NFCManager::readTagUID(uint8_t* uid, uint8_t* uidLength, uint32_t timeoutMs) {
#ifdef HAS_NFC
  if (!isAvailable() || nfcInstance == nullptr) {
    return false;
  }
  
  // Si la détection automatique est active et un tag est présent, utiliser les données en cache
  if (autoDetectEnabled && isTagPresent()) {
    return getLastTagUID(uid, uidLength);
  }
  
  // Sinon, faire une lecture manuelle (avec mutex pour éviter les conflits avec le thread)
  unsigned long startTime = millis();
  
  while (millis() - startTime < timeoutMs) {
    if (xSemaphoreTake(nfcMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      uint8_t success = nfcInstance->readPassiveTargetID(
        PN532_MIFARE_ISO14443A, 
        uid, 
        uidLength, 
        100  // Timeout court
      );
      
      xSemaphoreGive(nfcMutex);
      
      if (success) {
        if (*uidLength > 10) *uidLength = 10;
        return true;
      }
    }
    
    // Yield pour laisser les autres tâches s'exécuter
    vTaskDelay(pdMS_TO_TICKS(50));
  }
  
  return false;
#else
  return false;
#endif // HAS_NFC
}

bool NFCManager::readBlock(uint8_t blockNumber, uint8_t* data, uint8_t* uid, uint8_t uidLength) {
#ifdef HAS_NFC
  if (!isAvailable() || nfcInstance == nullptr) {
    return false;
  }
  
  if (blockNumber > 63) {
    return false;
  }
  
  bool result = false;
  
  if (xSemaphoreTake(nfcMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
    // Authentifier avec la clé par défaut
    uint8_t keyA[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    
    uint8_t success = nfcInstance->mifareclassic_AuthenticateBlock(uid, uidLength, blockNumber, 0, keyA);
    
    if (success) {
      // Lire le bloc
      success = nfcInstance->mifareclassic_ReadDataBlock(blockNumber, data);
      result = (success > 0);
    }
    
    xSemaphoreGive(nfcMutex);
  }
  
  return result;
#else
  return false;
#endif // HAS_NFC
}

bool NFCManager::writeBlock(uint8_t blockNumber, uint8_t* data, uint8_t* uid, uint8_t uidLength) {
#ifdef HAS_NFC
  if (!isAvailable() || nfcInstance == nullptr) {
    return false;
  }

  if (blockNumber > 63) {
    return false;
  }

  bool result = false;

  if (xSemaphoreTake(nfcMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
    // Authentifier avec la clé par défaut
    uint8_t keyA[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

    uint8_t success = nfcInstance->mifareclassic_AuthenticateBlock(uid, uidLength, blockNumber, 0, keyA);

    if (success) {
      // Écrire le bloc
      success = nfcInstance->mifareclassic_WriteDataBlock(blockNumber, data);
      result = (success > 0);
    }

    xSemaphoreGive(nfcMutex);
  }

  return result;
#else
  return false;
#endif // HAS_NFC
}

bool NFCManager::writeTag(const String& key, int variantCode) {
#ifdef HAS_NFC
  if (!isAvailable()) {
    Serial.println("[NFC] NFC non disponible");
    return false;
  }

  Serial.println("[NFC] Veuillez placer un tag NFC...");

  // Lire l'UID du tag
  uint8_t uid[10];
  uint8_t uidLength;

  if (!readTagUID(uid, &uidLength, 5000)) {
    Serial.println("[NFC] Aucun tag detecte");
    return false;
  }

  // Afficher l'UID du tag
  Serial.print("[NFC] Tag detecte - UID: ");
  for (uint8_t i = 0; i < uidLength; i++) {
    if (uid[i] < 0x10) Serial.print("0");
    Serial.print(uid[i], HEX);
    if (i < uidLength - 1) Serial.print(":");
  }
  Serial.println();

  // Préparer les données (16 bytes pour un bloc MIFARE)
  uint8_t data[16];
  memset(data, 0, 16);

  if (variantCode >= 1 && variantCode <= 10) {
    // Écrire uniquement le code variant (1 octet) : reconnaissance fiable, pas de corruption texte
    data[0] = (uint8_t)variantCode;
    Serial.printf("[NFC] Ecriture du code variant %d sur le bloc 4...\n", variantCode);
  } else {
    // Fallback : écrire la clé en texte (tronquer à 16 caractères)
    int len = key.length();
    if (len > 16) len = 16;
    memcpy(data, key.c_str(), len);
    Serial.print("[NFC] Ecriture de la cle '");
    Serial.print(key);
    Serial.println("' sur le bloc 4...");
  }

  bool success = writeBlock(4, data, uid, uidLength);

  if (success) {
    Serial.println("[NFC] Ecriture reussie!");
    return true;
  } else {
    Serial.println("[NFC] Erreur lors de l'ecriture");
    return false;
  }
#else
  Serial.println("[NFC] NFC non disponible sur ce modele");
  return false;
#endif // HAS_NFC
}
