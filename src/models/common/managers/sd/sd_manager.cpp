#include "sd_manager.h"
#include <SPI.h>
#include <SD.h>
#include <ArduinoJson.h>
#include "../../../model_config.h"
#include "../../config/core_config.h"

// Variables statiques
bool SDManager::initialized = false;
bool SDManager::cardAvailable = false;
const char* SDManager::CONFIG_FILE_PATH = "/config.json";

// Initialiser une configuration avec les valeurs par défaut
void SDManager::initDefaultConfig(SDConfig* config) {
  if (config == nullptr) return;
  
  config->valid = false; // Par défaut, pas de config depuis SD
  strcpy(config->device_name, DEFAULT_DEVICE_NAME);
  strcpy(config->wifi_ssid, DEFAULT_WIFI_SSID);
  strcpy(config->wifi_password, DEFAULT_WIFI_PASSWORD);
  config->led_brightness = DEFAULT_LED_BRIGHTNESS;
  config->sleep_timeout_ms = DEFAULT_SLEEP_TIMEOUT_MS;
  // Valeurs par défaut pour bedtime (modèle Dream)
  config->bedtime_colorR = 255;
  config->bedtime_colorG = 107;
  config->bedtime_colorB = 107;
  config->bedtime_brightness = 50;
  config->bedtime_allNight = false;
  strcpy(config->bedtime_effect, "none"); // Par défaut, couleur fixe
  strcpy(config->bedtime_weekdaySchedule, "{}"); // JSON vide par défaut
  // Valeurs par défaut pour wakeup (modèle Dream)
  config->wakeup_colorR = 255;
  config->wakeup_colorG = 200;
  config->wakeup_colorB = 100;
  config->wakeup_brightness = 50;
  strcpy(config->wakeup_weekdaySchedule, "{}"); // JSON vide par défaut
}

bool SDManager::init() {
  if (initialized) {
    return cardAvailable;
  }
  
  initialized = true;
  cardAvailable = false;
  
  if (initSDCard()) {
    cardAvailable = true;
    
    // Vérifier le type de carte
    uint8_t cardType = getCardType();
    if (cardType == CARD_NONE) {
      cardAvailable = false;
    }
  }
  
  return cardAvailable;
}

bool SDManager::isAvailable() {
  if (!initialized) {
    return false;
  }
  
  // Vérifier à nouveau si la carte est toujours présente
  if (cardAvailable) {
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
      cardAvailable = false;
    }
  }
  
  return cardAvailable;
}

uint8_t SDManager::getCardType() {
  if (!initialized) {
    return CARD_NONE;
  }
  return SD.cardType();
}

uint64_t SDManager::getTotalSpace() {
  if (!isAvailable()) {
    return 0;
  }
  return SD.totalBytes();
}

uint64_t SDManager::getFreeSpace() {
  if (!isAvailable()) {
    return 0;
  }
  return SD.totalBytes() - SD.usedBytes();
}

uint64_t SDManager::getUsedSpace() {
  if (!isAvailable()) {
    return 0;
  }
  return SD.usedBytes();
}

bool SDManager::initSDCard() {
  Serial.println("[SD] Initialisation carte SD...");
  Serial.printf("[SD] Pins: MOSI=%d, MISO=%d, SCK=%d, CS=%d\n", SD_MOSI_PIN, SD_MISO_PIN, SD_SCK_PIN, SD_CS_PIN);
  
  // Configurer la broche CS en sortie et la mettre à HIGH (désélectionner la carte)
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  delay(10);  // Petit délai pour stabiliser
  
  // Initialiser le bus SPI avec les pins configurés
  Serial.println("[SD] Initialisation bus SPI...");
  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN);
  delay(100);  // Délai plus long après SPI.begin() - important pour ESP32-C3
  
  // Initialiser la carte SD avec une fréquence plus basse pour ESP32-C3 (plus stable)
  // ESP32-C3 a des problèmes connus avec SD.begin() - utiliser une fréquence réduite
  #if defined(ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(IS_SINGLE_CORE)
    Serial.println("[SD] Mode ESP32-C3 detecte - utilisation frequence reduite (400kHz)");
    // ESP32-C3 : essayer plusieurs fréquences de plus en plus basses
    // Commencer par 400kHz (très basse mais plus stable)
    if (SD.begin(SD_CS_PIN, SPI, 400000)) {
      Serial.println("[SD] Carte SD initialisee (ESP32-C3, 400kHz)");
      return true;
    }
    Serial.println("[SD] Echec a 400kHz, essai a 250kHz...");
    delay(100);
    if (SD.begin(SD_CS_PIN, SPI, 250000)) {
      Serial.println("[SD] Carte SD initialisee (ESP32-C3, 250kHz)");
      return true;
    }
    Serial.println("[SD] Echec a 250kHz, essai frequence par defaut...");
    delay(100);
    // Dernier essai avec la fréquence par défaut
    if (SD.begin(SD_CS_PIN)) {
      Serial.println("[SD] Carte SD initialisee (ESP32-C3, defaut)");
      return true;
    }
  #else
    // ESP32-S3 : fréquence par défaut (plus rapide)
    Serial.println("[SD] Mode ESP32-S3 - frequence par defaut");
    if (SD.begin(SD_CS_PIN)) {
      Serial.println("[SD] Carte SD initialisee");
      return true;
    }
  #endif
  
  // Diagnostic supplémentaire
  Serial.println("[SD] ERREUR: Impossible d'initialiser la carte SD");
  Serial.println("[SD] Verifier les connexions et que la carte SD est formatee en FAT32");
  Serial.println("[SD] Diagnostic:");
  Serial.printf("[SD]   - Pin CS (GPIO %d) etat: %s\n", SD_CS_PIN, digitalRead(SD_CS_PIN) ? "HIGH" : "LOW");
  Serial.println("[SD]   - Verifier que la carte SD est bien connectee et formatee en FAT32");
  Serial.println("[SD]   - Sur ESP32-C3, les pins 4-7 sont partages avec JTAG (peut causer conflits)");
  return false;
}

bool SDManager::configFileExists() {
  if (!isAvailable()) {
    return false;
  }
  
  return SD.exists(CONFIG_FILE_PATH);
}

SDConfig SDManager::getConfig() {
  // Initialiser avec les valeurs par défaut
  SDConfig config;
  SDManager::initDefaultConfig(&config);
  
  if (!isAvailable()) {
    return config;
  }
  
  if (!configFileExists()) {
    return config;
  }
  
  // Ouvrir le fichier
  File configFile = SD.open(CONFIG_FILE_PATH, FILE_READ);
  if (!configFile) {
    return config;
  }
  
  // Lire la taille du fichier
  size_t fileSize = configFile.size();
  if (fileSize == 0) {
    configFile.close();
    return config;
  }
  
  // Allouer un buffer pour lire le fichier
  // Taille augmentée pour inclure weekdaySchedule (peut être volumineux avec 7 jours)
  const size_t maxSize = 1536;
  if (fileSize > maxSize) {
    fileSize = maxSize;
  }
  
  char* jsonBuffer = new char[fileSize + 1];
  if (!jsonBuffer) {
    configFile.close();
    return config;
  }
  
  // Lire le fichier
  size_t bytesRead = configFile.readBytes(jsonBuffer, fileSize);
  jsonBuffer[bytesRead] = '\0';
  configFile.close();
  
  if (bytesRead == 0) {
    delete[] jsonBuffer;
    return config;
  }
  
  // Parser le JSON
  // Utiliser StaticJsonDocument avec allocation statique (1536 octets pour inclure weekdaySchedule)
  // Note: StaticJsonDocument est déprécié mais toujours fonctionnel dans ArduinoJson v7
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  StaticJsonDocument<1536> doc;
  #pragma GCC diagnostic pop
  DeserializationError error = deserializeJson(doc, jsonBuffer);
  
  delete[] jsonBuffer;
  
  if (error) {
    return config;
  }
  
  // Extraire les valeurs (utiliser is<String>() au lieu de containsKey())
  if (doc["wifi_ssid"].is<String>()) {
    strncpy(config.wifi_ssid, doc["wifi_ssid"] | "", sizeof(config.wifi_ssid) - 1);
    config.wifi_ssid[sizeof(config.wifi_ssid) - 1] = '\0';
  }
  
  if (doc["wifi_password"].is<String>()) {
    strncpy(config.wifi_password, doc["wifi_password"] | "", sizeof(config.wifi_password) - 1);
    config.wifi_password[sizeof(config.wifi_password) - 1] = '\0';
  }
  
  if (doc["device_name"].is<String>()) {
    strncpy(config.device_name, doc["device_name"] | "Kidoo", sizeof(config.device_name) - 1);
    config.device_name[sizeof(config.device_name) - 1] = '\0';
  }
  
  if (doc["led_brightness"].is<int>()) {
    int brightness = doc["led_brightness"] | 255;
    if (brightness < 0) brightness = 0;
    if (brightness > 255) brightness = 255;
    config.led_brightness = (uint8_t)brightness;
  }
  
  if (doc["sleep_timeout_ms"].is<int>()) {
    int timeout = doc["sleep_timeout_ms"] | 0;
    if (timeout < 0) {
      timeout = 0;  // 0 = désactivé
    } else if (timeout > 0 && timeout < MIN_SLEEP_TIMEOUT_MS) {
      // Si activé mais en dessous du minimum, utiliser le minimum
      timeout = MIN_SLEEP_TIMEOUT_MS;
    }
    config.sleep_timeout_ms = (uint32_t)timeout;
  }
  
  // Configuration bedtime (modèle Dream)
  if (doc["bedtime_colorR"].is<int>()) {
    int colorR = doc["bedtime_colorR"] | 255;
    if (colorR < 0) colorR = 0;
    if (colorR > 255) colorR = 255;
    config.bedtime_colorR = (uint8_t)colorR;
  }
  
  if (doc["bedtime_colorG"].is<int>()) {
    int colorG = doc["bedtime_colorG"] | 107;
    if (colorG < 0) colorG = 0;
    if (colorG > 255) colorG = 255;
    config.bedtime_colorG = (uint8_t)colorG;
  }
  
  if (doc["bedtime_colorB"].is<int>()) {
    int colorB = doc["bedtime_colorB"] | 107;
    if (colorB < 0) colorB = 0;
    if (colorB > 255) colorB = 255;
    config.bedtime_colorB = (uint8_t)colorB;
  }
  
  if (doc["bedtime_brightness"].is<int>()) {
    int brightness = doc["bedtime_brightness"] | 50;
    if (brightness < 0) brightness = 0;
    if (brightness > 100) brightness = 100;
    config.bedtime_brightness = (uint8_t)brightness;
  }
  
  if (doc["bedtime_allNight"].is<bool>()) {
    config.bedtime_allNight = doc["bedtime_allNight"] | false;
  }
  
  // Lire l'effet bedtime
  if (doc["bedtime_effect"].is<String>()) {
    String effectStr = doc["bedtime_effect"] | "none";
    strncpy(config.bedtime_effect, effectStr.c_str(), sizeof(config.bedtime_effect) - 1);
    config.bedtime_effect[sizeof(config.bedtime_effect) - 1] = '\0';
  } else {
    // Par défaut, couleur fixe
    strcpy(config.bedtime_effect, "none");
  }
  
  // Lire weekdaySchedule (JSON sérialisé)
  if (doc["bedtime_weekdaySchedule"].is<String>()) {
    String scheduleStr = doc["bedtime_weekdaySchedule"] | "{}";
    strncpy(config.bedtime_weekdaySchedule, scheduleStr.c_str(), sizeof(config.bedtime_weekdaySchedule) - 1);
    config.bedtime_weekdaySchedule[sizeof(config.bedtime_weekdaySchedule) - 1] = '\0';
  } else if (doc["bedtime_weekdaySchedule"].is<JsonObject>()) {
    // Si c'est un objet JSON, le sérialiser en string
    String scheduleStr;
    serializeJson(doc["bedtime_weekdaySchedule"], scheduleStr);
    strncpy(config.bedtime_weekdaySchedule, scheduleStr.c_str(), sizeof(config.bedtime_weekdaySchedule) - 1);
    config.bedtime_weekdaySchedule[sizeof(config.bedtime_weekdaySchedule) - 1] = '\0';
  }
  
  // Configuration wakeup (modèle Dream)
  if (doc["wakeup_colorR"].is<int>()) {
    int colorR = doc["wakeup_colorR"] | 255;
    if (colorR >= 0 && colorR <= 255) {
      config.wakeup_colorR = (uint8_t)colorR;
    }
  }
  if (doc["wakeup_colorG"].is<int>()) {
    int colorG = doc["wakeup_colorG"] | 200;
    if (colorG >= 0 && colorG <= 255) {
      config.wakeup_colorG = (uint8_t)colorG;
    }
  }
  if (doc["wakeup_colorB"].is<int>()) {
    int colorB = doc["wakeup_colorB"] | 100;
    if (colorB >= 0 && colorB <= 255) {
      config.wakeup_colorB = (uint8_t)colorB;
    }
  }
  if (doc["wakeup_brightness"].is<int>()) {
    int brightness = doc["wakeup_brightness"] | 50;
    if (brightness >= 0 && brightness <= 100) {
      config.wakeup_brightness = (uint8_t)brightness;
    }
  }
  
  // Lire weekdaySchedule wakeup (JSON sérialisé)
  if (doc["wakeup_weekdaySchedule"].is<String>()) {
    String scheduleStr = doc["wakeup_weekdaySchedule"] | "{}";
    strncpy(config.wakeup_weekdaySchedule, scheduleStr.c_str(), sizeof(config.wakeup_weekdaySchedule) - 1);
    config.wakeup_weekdaySchedule[sizeof(config.wakeup_weekdaySchedule) - 1] = '\0';
  } else if (doc["wakeup_weekdaySchedule"].is<JsonObject>()) {
    // Si c'est un objet JSON, le sérialiser en string
    String scheduleStr;
    serializeJson(doc["wakeup_weekdaySchedule"], scheduleStr);
    strncpy(config.wakeup_weekdaySchedule, scheduleStr.c_str(), sizeof(config.wakeup_weekdaySchedule) - 1);
    config.wakeup_weekdaySchedule[sizeof(config.wakeup_weekdaySchedule) - 1] = '\0';
  }
  
  config.valid = true;
  return config;
}

bool SDManager::saveConfig(const SDConfig& config) {
  if (!isAvailable()) {
    return false;
  }
  
  // Créer un document JSON
  // Note: StaticJsonDocument est déprécié mais toujours fonctionnel dans ArduinoJson v7
  // Taille augmentée pour inclure weekdaySchedule (peut être volumineux avec 7 jours)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  StaticJsonDocument<1536> doc;
  #pragma GCC diagnostic pop
  
  // Ajouter les valeurs (seulement si elles sont valides/modifiées)
  if (strlen(config.device_name) > 0) {
    doc["device_name"] = config.device_name;
  }
  
  if (strlen(config.wifi_ssid) > 0) {
    doc["wifi_ssid"] = config.wifi_ssid;
  }
  
  if (strlen(config.wifi_password) > 0) {
    doc["wifi_password"] = config.wifi_password;
  }
  
  doc["led_brightness"] = config.led_brightness;
  doc["sleep_timeout_ms"] = config.sleep_timeout_ms;
  
  // Configuration bedtime (modèle Dream)
  doc["bedtime_colorR"] = config.bedtime_colorR;
  doc["bedtime_colorG"] = config.bedtime_colorG;
  doc["bedtime_colorB"] = config.bedtime_colorB;
  doc["bedtime_brightness"] = config.bedtime_brightness;
  doc["bedtime_allNight"] = config.bedtime_allNight;
  doc["bedtime_effect"] = config.bedtime_effect;
  
  // Sauvegarder weekdaySchedule (JSON sérialisé)
  // Essayer de parser le JSON pour valider qu'il est valide avant de le sauvegarder
  if (strlen(config.bedtime_weekdaySchedule) > 0) {
    // Parser le JSON pour valider qu'il est valide
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    StaticJsonDocument<512> scheduleDoc;
    #pragma GCC diagnostic pop
    DeserializationError error = deserializeJson(scheduleDoc, config.bedtime_weekdaySchedule);
    if (!error) {
      // JSON valide, le sauvegarder comme objet JSON dans le document
      doc["bedtime_weekdaySchedule"] = scheduleDoc;
    } else {
      // JSON invalide, sauvegarder comme string brute
      doc["bedtime_weekdaySchedule"] = config.bedtime_weekdaySchedule;
    }
  } else {
    // Vide, sauvegarder un objet vide
    doc["bedtime_weekdaySchedule"] = "{}";
  }
  
  // Configuration wakeup (modèle Dream)
  doc["wakeup_colorR"] = config.wakeup_colorR;
  doc["wakeup_colorG"] = config.wakeup_colorG;
  doc["wakeup_colorB"] = config.wakeup_colorB;
  doc["wakeup_brightness"] = config.wakeup_brightness;
  
  // Sauvegarder weekdaySchedule wakeup (JSON sérialisé)
  if (strlen(config.wakeup_weekdaySchedule) > 0) {
    // Parser le JSON pour valider qu'il est valide
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    StaticJsonDocument<512> scheduleDoc;
    #pragma GCC diagnostic pop
    DeserializationError error = deserializeJson(scheduleDoc, config.wakeup_weekdaySchedule);
    if (!error) {
      // JSON valide, le sauvegarder comme objet JSON dans le document
      doc["wakeup_weekdaySchedule"] = scheduleDoc;
    } else {
      // JSON invalide, sauvegarder comme string brute
      doc["wakeup_weekdaySchedule"] = config.wakeup_weekdaySchedule;
    }
  } else {
    // Vide, sauvegarder un objet vide
    doc["wakeup_weekdaySchedule"] = "{}";
  }
  
  // Ouvrir le fichier en mode écriture (crée le fichier s'il n'existe pas)
  File configFile = SD.open(CONFIG_FILE_PATH, FILE_WRITE);
  if (!configFile) {
    return false;
  }
  
  // Sérialiser le JSON dans le fichier
  size_t bytesWritten = serializeJson(doc, configFile);
  configFile.close();
  
  return (bytesWritten > 0);
}
