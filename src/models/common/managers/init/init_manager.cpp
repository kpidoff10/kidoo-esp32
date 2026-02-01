#include "init_manager.h"
#include "../led/led_manager.h"
#include "../sd/sd_manager.h"
#include "../serial/serial_manager.h"
#include "../log/log_manager.h"
#include "../nfc/nfc_manager.h"
#include "../ble/ble_manager.h"
#include "../ble_config/ble_config_manager.h"
#include "../wifi/wifi_manager.h"
#include "../pubnub/pubnub_manager.h"
#include "../rtc/rtc_manager.h"
#include "../potentiometer/potentiometer_manager.h"
#ifdef HAS_AUDIO
#include "../audio/audio_manager.h"
#endif
#include "../../../model_config.h"
#include "../../../../../color/colors.h"
#include "../../../model_init.h"

// Variables statiques
SystemStatus InitManager::systemStatus = {
  INIT_NOT_STARTED,  // serial
  INIT_NOT_STARTED,  // led
  INIT_NOT_STARTED,  // sd
  INIT_NOT_STARTED,  // nfc
  INIT_NOT_STARTED,  // ble
  INIT_NOT_STARTED,  // wifi
  INIT_NOT_STARTED,  // pubnub
  INIT_NOT_STARTED,  // rtc
  INIT_NOT_STARTED,  // potentiometer
  INIT_NOT_STARTED   // audio
};
bool InitManager::initialized = false;
SDConfig* InitManager::globalConfig = nullptr;

bool InitManager::init() {
  // 1. Initialiser la communication série EN PREMIER (priorité absolue)
  // On ne peut pas utiliser Serial.println avant !
  bool serialAvailable = initSerial();
  
  // Si Serial est disponible, initialiser les managers qui en dépendent
  if (serialAvailable) {
    // Initialiser le SerialManager après que Serial soit prêt
    SerialManager::init();
    
    // Initialiser le LogManager après que Serial et SD soient prêts
    LogManager::init();
    
    Serial.println("");
    Serial.println("========================================");
    Serial.print("     KIDOO ESP32 ");
    Serial.print(KIDOO_MODEL_NAME);
    Serial.println(" - DEMARRAGE");
    Serial.println("========================================");
    Serial.println("");
  }
  // Si Serial n'est pas disponible (USB non connecté), continuer quand même
  // Le système peut fonctionner sans Serial
  
  // Vérifier si déjà initialisé
  if (initialized) {
    return true;
  }
  
  // Configuration spécifique au modèle (avant l'initialisation des composants)
  if (!InitModel::configure()) {
    if (serialAvailable) {
      Serial.println("[INIT] ERREUR: Configuration modele echouee");
    }
    return false;
  }
  
  bool allSuccess = true;
  bool bleAutoActivated = false;  // BLE activé automatiquement (pas de WiFi) -> pas de retour lumineux
  
  // ÉTAPE 1 : Initialiser la carte SD et récupérer la configuration (CRITIQUE)
  if (!initSD()) {
    if (serialAvailable) {
      Serial.println("[INIT] ERREUR: Carte SD non disponible");
    }
    
    // Initialiser les LEDs en mode d'erreur (respiration rouge) si disponibles
    #ifdef HAS_LED
    if (HAS_LED && initLED()) {
      LEDManager::setColor(COLOR_ERROR);
      LEDManager::setEffect(LED_EFFECT_PULSE);
    }
    #endif
    
    initialized = true;
    return false;
  }
  delay(100);
  
  // Détecter sortie d'usine (carte SD neuve = pas de config.json) pour BLE + cercle bleu auto
  bool configFileExists = SDManager::configFileExists();
  if (!configFileExists && serialAvailable) {
    Serial.println("[INIT] Pas de config.json (carte neuve / sortie d'usine)");
  }
  
  // ÉTAPE 2 : Initialiser le gestionnaire LED
  #ifdef HAS_LED
  if (HAS_LED) {
    if (!initLED()) {
      if (serialAvailable) {
        Serial.println("[INIT] ERREUR: Echec LED");
      }
      allSuccess = false;
    }
    delay(100);
  }
  #endif
  
  // ÉTAPE 3 : Initialiser le gestionnaire NFC (optionnel)
  #ifdef HAS_NFC
  if (HAS_NFC) {
    initNFC();  // Affiche un WARNING si non opérationnel, mais n'empêche pas l'initialisation
    delay(100);
  }
  #endif
  
  // ÉTAPE 4 : Initialiser le BLE (après l'init complète)
  #ifdef HAS_BLE
  if (HAS_BLE) {
    initBLE();  // Affiche un WARNING si non opérationnel, mais n'empêche pas l'initialisation
    delay(100);
  }
  #endif
  
  // ÉTAPE 5 : Initialiser le WiFi et se connecter si configuré
  #ifdef HAS_WIFI
  if (HAS_WIFI) {
    initWiFi();  // Tente de se connecter au WiFi configuré dans config.json
    
    // Sortie d'usine (pas de config.json) : pas d'attente WiFi, BLE + mode cercle bleu direct
    if (!configFileExists) {
      #ifdef HAS_BLE
      if (HAS_BLE && BLEConfigManager::isInitialized()) {
        if (serialAvailable) {
          Serial.println("");
          Serial.println("[INIT] ========================================");
          Serial.println("[INIT] Sortie d'usine - pas de configuration");
          Serial.println("[INIT] Activation BLE + mode cercle bleu pour configuration");
          Serial.println("[INIT] BLE actif pendant 15 minutes (timeout automatique)");
          Serial.println("[INIT] ========================================");
        }
        BLEConfigManager::enableBLE(0, true);  // Avec feedback = cercle bleu (reconnaissance)
        bleAutoActivated = true;
      }
      #endif
    } else {
      // Config existante : attendre 8 s pour voir si le WiFi se connecte
      if (serialAvailable) {
        Serial.println("[INIT] Attente de connexion WiFi (8 secondes)...");
      }
      unsigned long wifiWaitStart = millis();
      const unsigned long WIFI_WAIT_TIMEOUT_MS = 8000;  // 8 secondes
      
      while ((millis() - wifiWaitStart) < WIFI_WAIT_TIMEOUT_MS) {
        if (WiFiManager::isConnected()) {
          if (serialAvailable) {
            Serial.println("[INIT] WiFi connecte - BLE ne sera pas active automatiquement");
          }
          break;
        }
        delay(500);  // Vérifier toutes les 500ms
      }
      
      delay(100);
      
      // Si le WiFi n'est toujours pas connecté après l'attente, activer le BLE SANS cercle bleu
      #ifdef HAS_BLE
      if (HAS_BLE && BLEConfigManager::isInitialized()) {
        if (!WiFiManager::isConnected()) {
          if (serialAvailable) {
            Serial.println("");
            Serial.println("[INIT] ========================================");
            Serial.println("[INIT] WiFi non connecte apres attente");
            Serial.println("[INIT] Activation automatique du BLE pour configuration");
            Serial.println("[INIT] BLE actif pendant 15 minutes (timeout automatique)");
            Serial.println("[INIT] ========================================");
          }
          BLEConfigManager::enableBLE(0, false);  // SANS feedback lumineux
          bleAutoActivated = true;
        } else {
          if (serialAvailable) {
            Serial.println("[INIT] WiFi connecte - BLE non active automatiquement");
          }
        }
      }
      #endif
    }
  }
  #endif
  
  // ÉTAPE 6 : Initialiser PubNub (après WiFi)
  #ifdef HAS_PUBNUB
  if (HAS_PUBNUB) {
    initPubNub();  // Tente de se connecter à PubNub
    delay(100);
  }
  #endif
  
  // ÉTAPE 7 : Initialiser le RTC DS3231 (optionnel)
  #ifdef HAS_RTC
  if (HAS_RTC) {
    initRTC();  // Affiche un WARNING si non opérationnel
    delay(100);
  }
  #endif
  
  // ÉTAPE 8 : Initialiser le potentiomètre (optionnel)
  #ifdef HAS_POTENTIOMETER
  if (HAS_POTENTIOMETER) {
    initPotentiometer();  // Pour contrôle du volume/luminosité
  }
  #endif
  
  // ÉTAPE 9 : Initialiser l'audio I2S (optionnel)
  #ifdef HAS_AUDIO
  if (HAS_AUDIO) {
    initAudio();  // Lecteur audio depuis SD
    delay(100);
  }
  #endif
  
  // ÉTAPE 10 : Initialisation spécifique au modèle (APRÈS tous les composants)
  if (serialAvailable) {
    Serial.println("[INIT] Appel InitModel::init()...");
  }
  if (!InitModel::init()) {
    if (serialAvailable) {
      Serial.println("[INIT] ERREUR: Initialisation modele echouee");
    }
    allSuccess = false;
  }
  delay(100);
  
  initialized = true;
  
  if (allSuccess) {
    if (serialAvailable) {
      Serial.println("[INIT] OK");
    }
    // Mettre les LEDs en vert qui tourne pour indiquer que tout est OK
    // SAUF si BLE auto (pas de WiFi) : pas de retour lumineux, LEDs restent éteintes
    #ifdef HAS_LED
    if (HAS_LED && systemStatus.led == INIT_SUCCESS && !bleAutoActivated) {
      // Ne pas réveiller les LEDs si elles sont déjà en sleep mode
      // (par exemple si le timeout de sleep est très court)
      if (!LEDManager::getSleepState()) {
        LEDManager::setColor(COLOR_SUCCESS);
        LEDManager::setEffect(LED_EFFECT_ROTATE);
      } else {
        if (serialAvailable) {
          Serial.println("[INIT] LEDs en sleep mode - pas d'affichage vert");
        }
      }
    }
    #endif
  } else {
    if (serialAvailable) {
      Serial.println("[INIT] ERREUR");
      printStatus();
    }
  }
  
  return allSuccess;
}

// Les fonctions d'initialisation communes sont dans models/common/init/
// models/common/init/init_serial.cpp, models/common/init/init_sd.cpp, models/common/init/init_led.cpp, models/common/init/init_nfc.cpp, models/common/init/init_ble.cpp, models/common/init/init_wifi.cpp, models/common/init/init_pubnub.cpp

SystemStatus InitManager::getStatus() {
  return systemStatus;
}

bool InitManager::isSystemReady() {
  return systemStatus.isFullyInitialized();
}

InitStatus InitManager::getComponentStatus(const char* componentName) {
  if (strcmp(componentName, "serial") == 0) {
    return systemStatus.serial;
  } else if (strcmp(componentName, "led") == 0) {
    return systemStatus.led;
  } else if (strcmp(componentName, "sd") == 0) {
    return systemStatus.sd;
  } else if (strcmp(componentName, "nfc") == 0) {
    return systemStatus.nfc;
  } else if (strcmp(componentName, "ble") == 0) {
    return systemStatus.ble;
  } else if (strcmp(componentName, "wifi") == 0) {
    return systemStatus.wifi;
  } else if (strcmp(componentName, "pubnub") == 0) {
    return systemStatus.pubnub;
  } else if (strcmp(componentName, "rtc") == 0) {
    return systemStatus.rtc;
  } else if (strcmp(componentName, "potentiometer") == 0) {
    return systemStatus.potentiometer;
  } else if (strcmp(componentName, "audio") == 0) {
    return systemStatus.audio;
  }
  // Ajouter d'autres composants ici
  
  return INIT_NOT_STARTED;
}

void InitManager::printStatus() {
  if (!Serial) {
    return;  // Serial non disponible, ne rien afficher
  }
  
  Serial.println("[INIT] ========== Statut du systeme ==========");
  
  Serial.print("[INIT] Serial: ");
  switch (systemStatus.serial) {
    case INIT_NOT_STARTED:
      Serial.println("Non demarre");
      break;
    case INIT_IN_PROGRESS:
      Serial.println("En cours");
      break;
    case INIT_SUCCESS:
      Serial.println("OK");
      break;
    case INIT_FAILED:
      Serial.println("ERREUR");
      break;
  }
  
  #ifdef HAS_LED
  if (HAS_LED) {
    Serial.print("[INIT] LED: ");
    switch (systemStatus.led) {
      case INIT_NOT_STARTED:
        Serial.println("Non demarre");
        break;
      case INIT_IN_PROGRESS:
        Serial.println("En cours");
        break;
      case INIT_SUCCESS:
        Serial.println("OK");
        break;
      case INIT_FAILED:
        Serial.println("ERREUR");
        break;
    }
  }
  #endif
  
  Serial.print("[INIT] SD: ");
  switch (systemStatus.sd) {
    case INIT_NOT_STARTED:
      Serial.println("Non demarre");
      break;
    case INIT_IN_PROGRESS:
      Serial.println("En cours");
      break;
    case INIT_SUCCESS:
      Serial.println("OK");
      break;
    case INIT_FAILED:
      Serial.println("ERREUR");
      break;
  }
  
  #ifdef HAS_NFC
  if (HAS_NFC) {
    Serial.print("[INIT] NFC: ");
    switch (systemStatus.nfc) {
      case INIT_NOT_STARTED:
        Serial.println("Non demarre");
        break;
      case INIT_IN_PROGRESS:
        Serial.println("En cours");
        break;
      case INIT_SUCCESS:
        Serial.println("OK");
        break;
      case INIT_FAILED:
        Serial.println("WARNING");
        break;
    }
  }
  #endif
  
  #ifdef HAS_BLE
  if (HAS_BLE) {
    Serial.print("[INIT] BLE: ");
    switch (systemStatus.ble) {
      case INIT_NOT_STARTED:
        Serial.println("Non demarre");
        break;
      case INIT_IN_PROGRESS:
        Serial.println("En cours");
        break;
      case INIT_SUCCESS:
        Serial.println("OK");
        break;
      case INIT_FAILED:
        Serial.println("ERREUR");
        break;
    }
  }
  #endif
  
  #ifdef HAS_WIFI
  if (HAS_WIFI) {
    Serial.print("[INIT] WiFi: ");
    switch (systemStatus.wifi) {
      case INIT_NOT_STARTED:
        Serial.println("Non demarre");
        break;
      case INIT_IN_PROGRESS:
        Serial.println("En cours");
        break;
      case INIT_SUCCESS:
        if (WiFiManager::isConnected()) {
          Serial.println("OK");
          Serial.print("[INIT]   -> IP: ");
          Serial.println(WiFiManager::getLocalIP());
        } else {
          Serial.println("OK (non connecte)");
        }
        break;
      case INIT_FAILED:
        Serial.println("ERREUR");
        break;
    }
  }
  #endif
  
  #ifdef HAS_PUBNUB
  if (HAS_PUBNUB) {
    Serial.print("[INIT] PubNub: ");
    switch (systemStatus.pubnub) {
      case INIT_NOT_STARTED:
        Serial.println("Non demarre");
        break;
      case INIT_IN_PROGRESS:
        Serial.println("En cours");
        break;
      case INIT_SUCCESS:
        Serial.println("OK");
        if (PubNubManager::isConnected()) {
          Serial.print("[INIT]   -> Channel: ");
          Serial.println(PubNubManager::getChannel());
        }
        break;
      case INIT_FAILED:
        Serial.println("Non configure");
        break;
    }
  }
  #endif
  
  #ifdef HAS_RTC
  if (HAS_RTC) {
    Serial.print("[INIT] RTC: ");
    switch (systemStatus.rtc) {
      case INIT_NOT_STARTED:
        Serial.println("Non demarre");
        break;
      case INIT_IN_PROGRESS:
        Serial.println("En cours");
        break;
      case INIT_SUCCESS:
        Serial.println("OK");
        Serial.print("[INIT]   -> Heure: ");
        Serial.println(RTCManager::getDateTimeString());
        break;
      case INIT_FAILED:
        Serial.println("Non disponible");
        break;
    }
  }
  #endif
  
  #ifdef HAS_POTENTIOMETER
  if (HAS_POTENTIOMETER) {
    Serial.print("[INIT] Potentiometre: ");
    switch (systemStatus.potentiometer) {
      case INIT_NOT_STARTED:
        Serial.println("Non demarre");
        break;
      case INIT_IN_PROGRESS:
        Serial.println("En cours");
        break;
      case INIT_SUCCESS:
        Serial.println("OK");
        Serial.print("[INIT]   -> Valeur: ");
        Serial.print(PotentiometerManager::getLastValue());
        Serial.println("%");
        break;
      case INIT_FAILED:
        Serial.println("Non disponible");
        break;
    }
  }
  #endif
  
  #ifdef HAS_AUDIO
  if (HAS_AUDIO) {
    Serial.print("[INIT] Audio: ");
    switch (systemStatus.audio) {
      case INIT_NOT_STARTED:
        Serial.println("Non demarre");
        break;
      case INIT_IN_PROGRESS:
        Serial.println("En cours");
        break;
      case INIT_SUCCESS:
        Serial.println("OK");
        Serial.print("[INIT]   -> Volume: ");
        Serial.print(AudioManager::getVolume());
        Serial.println("/21");
        break;
      case INIT_FAILED:
        Serial.println("Non disponible");
        break;
    }
  }
  #endif
  
  // Ajouter d'autres composants ici
  
  Serial.print("[INIT] Systeme pret: ");
  Serial.println(isSystemReady() ? "OUI" : "NON");
  Serial.println("[INIT] ========================================");
}

const SDConfig& InitManager::getConfig() {
  static SDConfig defaultConfig;
  
  if (globalConfig == nullptr) {
    // Si pas de config globale, retourner une config par défaut
    SDManager::initDefaultConfig(&defaultConfig);
    return defaultConfig;
  }
  
  return *globalConfig;
}

bool InitManager::isConfigValid() {
  return globalConfig != nullptr && globalConfig->valid;
}

void InitManager::setGlobalConfig(SDConfig* config) {
  globalConfig = config;
}

bool InitManager::updateConfig(const SDConfig& config) {
  if (globalConfig == nullptr) {
    return false;
  }
  
  // Copier la nouvelle configuration
  *globalConfig = config;
  
  // Sauvegarder sur la SD
  if (SDManager::isAvailable()) {
    return SDManager::saveConfig(config);
  }
  
  return false;
}
