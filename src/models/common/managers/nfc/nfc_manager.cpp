#include "nfc_manager.h"
#include "../../../model_config.h"
#include "../../config/core_config.h"
#include <Arduino.h>

#ifdef HAS_NFC
#include <Wire.h>
#include <Adafruit_PN532.h>
#include <string.h>

// Instance statique du PN532 (créée lors de l'initialisation)
static Adafruit_PN532* nfcInstance = nullptr;
#endif // HAS_NFC

// Variables statiques
bool NFCManager::initialized = false;
bool NFCManager::available = false;
uint32_t NFCManager::firmwareVersion = 0;

// Thread
TaskHandle_t NFCManager::taskHandle = nullptr;
SemaphoreHandle_t NFCManager::nfcMutex = nullptr;
volatile bool NFCManager::threadRunning = false;
volatile bool NFCManager::autoDetectEnabled = true;  // Actif par défaut

// Dernier tag détecté
uint8_t NFCManager::lastUID[10] = {0};
uint8_t NFCManager::lastUIDLength = 0;
volatile bool NFCManager::tagPresent = false;
unsigned long NFCManager::lastDetectionTime = 0;

// Callback
NFCTagCallback NFCManager::tagCallback = nullptr;

// Configuration du thread NFC
#define NFC_TASK_STACK_SIZE 4096
#define NFC_TASK_PRIORITY 2  // Priorité très basse (ne doit JAMAIS interférer avec l'audio)
#define NFC_SCAN_INTERVAL_MS 300  // Intervalle entre les scans (300ms) - plus espacé
#define NFC_TAG_TIMEOUT_MS 1500   // Timeout pour considérer qu'un tag est parti

#ifdef HAS_NFC
// =========================
// Thread NFC
// =========================
void NFCManager::nfcTask(void* parameter) {
  (void)parameter;
  
  Serial.printf("[NFC] Thread demarre sur Core %d\n", xPortGetCoreID());
  threadRunning = true;
  
  uint8_t uid[10];
  uint8_t uidLength;
  
  while (true) {
    // Vérifier si la détection automatique est activée
    if (autoDetectEnabled && nfcInstance != nullptr) {
      // Prendre le mutex pour accéder au hardware NFC
      if (xSemaphoreTake(nfcMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        // Essayer de détecter un tag (timeout TRÈS court de 50ms)
        uint8_t success = nfcInstance->readPassiveTargetID(
          PN532_MIFARE_ISO14443A, 
          uid, 
          &uidLength, 
          50  // Timeout très court pour ne pas bloquer l'audio
        );
        
        if (success) {
          // Limiter la longueur
          if (uidLength > 10) uidLength = 10;
          
          // Vérifier si c'est un nouveau tag ou le même
          bool isNewTag = (uidLength != lastUIDLength) || 
                          (memcmp(uid, lastUID, uidLength) != 0);
          
          // Mettre à jour les infos du tag
          memcpy(lastUID, uid, uidLength);
          lastUIDLength = uidLength;
          tagPresent = true;
          lastDetectionTime = millis();
          
          xSemaphoreGive(nfcMutex);  // Libérer le mutex d'abord
          
          // Appeler le callback si c'est un nouveau tag
          if (isNewTag) {
            // Afficher l'UID du tag détecté
            Serial.print("[NFC] Tag detecte: ");
            for (uint8_t i = 0; i < uidLength; i++) {
              if (lastUID[i] < 0x10) Serial.print("0");
              Serial.print(lastUID[i], HEX);
              if (i < uidLength - 1) Serial.print(":");
            }
            Serial.println();
            
            if (tagCallback != nullptr) {
              Serial.println("[NFC] Appel du callback...");
              tagCallback(lastUID, lastUIDLength);
            } else {
              Serial.println("[NFC] Pas de callback configure");
            }
          }
        } else {
          xSemaphoreGive(nfcMutex);
          
          // Pas de tag détecté
          if (tagPresent && (millis() - lastDetectionTime > NFC_TAG_TIMEOUT_MS)) {
            tagPresent = false;
            // Réinitialiser l'UID pour que le même tag soit détecté comme "nouveau" la prochaine fois
            lastUIDLength = 0;
            memset(lastUID, 0, sizeof(lastUID));
            Serial.println("[NFC] Tag retire");
          }
        }
      }
    }
    
    // Attendre avant le prochain scan
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
  
  // Initialiser le bus I2C
  Wire.begin(NFC_SDA_PIN, NFC_SCL_PIN);
  Wire.setTimeout(500);
  delay(100);
  
  Serial.printf("[NFC] Pins I2C: SDA=%d, SCL=%d\n", NFC_SDA_PIN, NFC_SCL_PIN);
  
  // Créer une instance temporaire du PN532 pour le test
  Adafruit_PN532 nfc(NFC_SDA_PIN, NFC_SCL_PIN);
  
  // Initialiser le module PN532
  nfc.begin();
  delay(200);
  
  // Tenter de lire la version du firmware
  uint32_t versiondata = nfc.getFirmwareVersion();
  
  if (!versiondata) {
    Serial.println("[NFC] Module PN532 non detecte");
    firmwareVersion = 0;
    return false;
  }
  
  // Le module est détecté
  firmwareVersion = versiondata;
  
  // Afficher les infos du firmware
  Serial.print("[NFC] Chip PN5");
  Serial.println((versiondata >> 24) & 0xFF, HEX);
  Serial.print("[NFC] Firmware: ");
  Serial.print((versiondata >> 16) & 0xFF, DEC);
  Serial.print(".");
  Serial.println((versiondata >> 8) & 0xFF, DEC);
  
  // Configurer le PN532
  nfc.SAMConfig();
  
  // Créer l'instance statique pour les opérations futures
  if (nfcInstance == nullptr) {
    nfcInstance = new Adafruit_PN532(NFC_SDA_PIN, NFC_SCL_PIN);
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
