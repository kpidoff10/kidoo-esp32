#include "init_manager.h"
#include "common/managers/led/led_manager.h"
#include "common/managers/sd/sd_manager.h"
#include "common/managers/serial/serial_manager.h"
#include "common/managers/log/log_manager.h"
#include "common/managers/nfc/nfc_manager.h"
#include "common/managers/ble/ble_manager.h"
#include "common/managers/ble_config/ble_config_manager.h"
#include "common/managers/wifi/wifi_manager.h"
#include "common/managers/pubnub/pubnub_manager.h"
#include "common/managers/ota/ota_manager.h"
#include "common/managers/rtc/rtc_manager.h"
#include "common/managers/potentiometer/potentiometer_manager.h"
#ifdef HAS_AUDIO
#include "common/managers/audio/audio_manager.h"
#endif
#ifdef HAS_VIBRATOR
#include "common/managers/vibrator/vibrator_manager.h"
#endif
#ifdef HAS_TOUCH
#include "common/managers/touch/touch_manager.h"
#endif
#include "models/model_config.h"
#include "color/colors.h"
#include "models/model_init.h"
#ifdef KIDOO_MODEL_DREAM
#include "models/dream/managers/bedtime/bedtime_manager.h"
#include "models/dream/managers/wakeup/wakeup_manager.h"
#endif

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
  INIT_NOT_STARTED,  // audio
  INIT_NOT_STARTED,  // vibrator
  INIT_NOT_STARTED,  // touch
  INIT_NOT_STARTED   // envSensor
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
    
    LogManager::info("");
    LogManager::info("========================================");
    LogManager::info("     KIDOO ESP32 %s - DEMARRAGE", KIDOO_MODEL_NAME);
    LogManager::info("========================================");
    LogManager::info("");
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
      LogManager::error("[INIT] Configuration modele echouee");
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
    LogManager::debug("[INIT] Pas de config.json (carte neuve / sortie d'usine)");
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
  
  // ÉTAPE 2b : Initialiser le LCD (modèle Gotchi avec HAS_LCD)
  #ifdef HAS_LCD
  if (HAS_LCD) {
    if (!initLCD()) {
      if (serialAvailable) {
        LogManager::warning("[INIT] LCD non disponible");
      }
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
          LogManager::debug("[INIT] Sortie d'usine - Activation BLE pour configuration");
        }
        BLEConfigManager::enableBLE(0, true);  // Avec feedback = cercle bleu (reconnaissance)
        bleAutoActivated = true;
      }
      #endif
    } else {
      // Config existante : attendre 8 s pour voir si le WiFi se connecte
      if (serialAvailable) {
        LogManager::debug("[INIT] Attente de connexion WiFi (8 secondes)...");
      }
      unsigned long wifiWaitStart = millis();
      const unsigned long WIFI_WAIT_TIMEOUT_MS = 8000;  // 8 secondes
      
      while ((millis() - wifiWaitStart) < WIFI_WAIT_TIMEOUT_MS) {
        if (WiFiManager::isConnected()) {
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
            LogManager::info("");
            LogManager::info("[INIT] ========================================");
            LogManager::info("[INIT] WiFi non connecte apres attente");
            LogManager::info("[INIT] Activation automatique du BLE pour configuration");
            LogManager::info("[INIT] BLE actif pendant 15 minutes (timeout automatique)");
            LogManager::info("[INIT] ========================================");
          }
          BLEConfigManager::enableBLE(0, false);  // SANS feedback lumineux
          bleAutoActivated = true;
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
  
  // ÉTAPE 9b : Initialiser le vibreur (optionnel)
  #ifdef HAS_VIBRATOR
  if (HAS_VIBRATOR) {
    initVibrator();
    delay(50);
  }
  #endif
  
  // ÉTAPE 9c : Initialiser le capteur tactile TTP223 (optionnel)
  #ifdef HAS_TOUCH
  if (HAS_TOUCH) {
    initTouch();
    delay(50);
  }
  #endif
  
  // ÉTAPE 9d : Initialiser le capteur environnemental AHT20 + BMP280 (optionnel)
  #ifdef HAS_ENV_SENSOR
  if (HAS_ENV_SENSOR) {
    initEnvSensor();
    delay(50);
  }
  #endif
  
  // ÉTAPE 10 : Initialisation spécifique au modèle (APRÈS tous les composants)
  if (serialAvailable) {
    LogManager::info("[INIT] Appel InitModel::init()...");
  }
  if (!InitModel::init()) {
    if (serialAvailable) {
      Serial.println("[INIT] ERREUR: Initialisation modele echouee");
    }
    allSuccess = false;
  }
  delay(100);
  
  initialized = true;

#ifdef HAS_PUBNUB
  OTAManager::publishLastOtaErrorIfAny();
#endif
  
  if (allSuccess) {
    if (serialAvailable) {
      LogManager::debug("[INIT] OK");
    }
    // Mettre les LEDs en vert qui tourne pour indiquer que tout est OK (prêt)
    // SAUF si BLE auto (pas de WiFi) : pas de retour lumineux, LEDs restent éteintes
    // Sur Dream : ne pas écraser l'affichage si bedtime ou wakeup est déjà actif au boot (plage coucher→lever ou fenêtre wakeup)
    #ifdef HAS_LED
    if (HAS_LED && systemStatus.led == INIT_SUCCESS && !bleAutoActivated) {
      // Ne pas réveiller les LEDs si elles sont déjà en sleep mode
      // (par exemple si le timeout de sleep est très court)
      if (!LEDManager::getSleepState()) {
#ifdef KIDOO_MODEL_DREAM
        if (!BedtimeManager::isBedtimeActive() && !WakeupManager::isWakeupActive()) {
#endif
          LEDManager::setColor(COLOR_GREEN);
          LEDManager::setEffect(LED_EFFECT_ROTATE);
#ifdef KIDOO_MODEL_DREAM
        }
#endif
      } else {
        if (serialAvailable) {
          Serial.println("[INIT] LEDs en sleep mode - pas d'affichage");
        }
      }
    }
    #endif
  } else {
    if (serialAvailable) {
      LogManager::error("[INIT] ERREUR");
      printStatus();
    }
  }
  
  return allSuccess;
}

// Les fonctions d'initialisation communes sont dans common/init/
// common/init/init_serial.cpp, init_sd.cpp, init_led.cpp, init_nfc.cpp, init_ble.cpp, init_wifi.cpp, init_pubnub.cpp

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
  } else if (strcmp(componentName, "vibrator") == 0) {
    return systemStatus.vibrator;
  } else if (strcmp(componentName, "touch") == 0) {
    return systemStatus.touch;
  } else if (strcmp(componentName, "envSensor") == 0) {
    return systemStatus.envSensor;
  }
  // Ajouter d'autres composants ici
  
  return INIT_NOT_STARTED;
}

void InitManager::printStatus() {
  if (!Serial) {
    return;  // Serial non disponible, ne rien afficher
  }
  
  // Serial fonctionne (on vient de passer le check ci-dessus).
  // Sur ESP32-C3 USB CDC, l'énumération peut être tardive au boot : mettre à jour le statut
  // pour que isSystemReady() soit cohérent.
  systemStatus.serial = INIT_SUCCESS;
  
  LogManager::info("[INIT] ========== Statut du systeme ==========");
  LogManager::info("[INIT] Serial: OK");
  
  #ifdef HAS_LED
  if (HAS_LED) {
    const char* ledStr = "?";
    switch (systemStatus.led) {
      case INIT_NOT_STARTED: ledStr = "Non demarre"; break;
      case INIT_IN_PROGRESS: ledStr = "En cours"; break;
      case INIT_SUCCESS: ledStr = "OK"; break;
      case INIT_FAILED: ledStr = "ERREUR"; break;
    }
    LogManager::info("[INIT] LED: %s", ledStr);
  }
  #endif
  
  const char* sdStr = "?";
  switch (systemStatus.sd) {
    case INIT_NOT_STARTED: sdStr = "Non demarre"; break;
    case INIT_IN_PROGRESS: sdStr = "En cours"; break;
    case INIT_SUCCESS: sdStr = "OK"; break;
    case INIT_FAILED: sdStr = "ERREUR"; break;
  }
  LogManager::info("[INIT] SD: %s", sdStr);
  
  #ifdef HAS_NFC
  if (HAS_NFC) {
    const char* nfcStr = "?";
    switch (systemStatus.nfc) {
      case INIT_NOT_STARTED: nfcStr = "Non demarre"; break;
      case INIT_IN_PROGRESS: nfcStr = "En cours"; break;
      case INIT_SUCCESS: nfcStr = "OK"; break;
      case INIT_FAILED: nfcStr = "WARNING"; break;
    }
    LogManager::info("[INIT] NFC: %s", nfcStr);
  }
  #endif
  
  #ifdef HAS_BLE
  if (HAS_BLE) {
    const char* bleStr = "?";
    switch (systemStatus.ble) {
      case INIT_NOT_STARTED: bleStr = "Non demarre"; break;
      case INIT_IN_PROGRESS: bleStr = "En cours"; break;
      case INIT_SUCCESS: bleStr = "OK"; break;
      case INIT_FAILED: bleStr = "ERREUR"; break;
    }
    LogManager::info("[INIT] BLE: %s", bleStr);
  }
  #endif
  
  #ifdef HAS_WIFI
  if (HAS_WIFI) {
    const char* wifiStr = "?";
    switch (systemStatus.wifi) {
      case INIT_NOT_STARTED: wifiStr = "Non demarre"; break;
      case INIT_IN_PROGRESS: wifiStr = "En cours"; break;
      case INIT_SUCCESS:
        LogManager::info("[INIT] WiFi: %s", WiFiManager::isConnected() ? "OK" : "OK (non connecte)");
        if (WiFiManager::isConnected()) {
          LogManager::info("[INIT]   -> IP: %s", WiFiManager::getLocalIP().c_str());
        }
        wifiStr = nullptr;  // Already logged
        break;
      case INIT_FAILED: wifiStr = "ERREUR"; break;
    }
    if (wifiStr != nullptr) {
      LogManager::info("[INIT] WiFi: %s", wifiStr);
    }
  }
  #endif
  
  #ifdef HAS_PUBNUB
  if (HAS_PUBNUB) {
    const char* pubnubStr = "?";
    switch (systemStatus.pubnub) {
      case INIT_NOT_STARTED: pubnubStr = "Non demarre"; break;
      case INIT_IN_PROGRESS: pubnubStr = "En cours"; break;
      case INIT_SUCCESS:
        LogManager::info("[INIT] PubNub: OK");
        if (PubNubManager::isConnected()) {
          LogManager::info("[INIT]   -> Channel: %s", PubNubManager::getChannel());
        }
        pubnubStr = nullptr;
        break;
      case INIT_FAILED: pubnubStr = "Non configure"; break;
    }
    if (pubnubStr != nullptr) {
      LogManager::info("[INIT] PubNub: %s", pubnubStr);
    }
  }
  #endif
  
  #ifdef HAS_RTC
  if (HAS_RTC) {
    const char* rtcStr = "?";
    switch (systemStatus.rtc) {
      case INIT_NOT_STARTED: rtcStr = "Non demarre"; break;
      case INIT_IN_PROGRESS: rtcStr = "En cours"; break;
      case INIT_SUCCESS:
        LogManager::info("[INIT] RTC: OK");
        LogManager::info("[INIT]   -> Heure: %s", RTCManager::getDateTimeString().c_str());
        rtcStr = nullptr;
        break;
      case INIT_FAILED: rtcStr = "Non disponible"; break;
    }
    if (rtcStr != nullptr) {
      LogManager::info("[INIT] RTC: %s", rtcStr);
    }
  }
  #endif
  
  #ifdef HAS_POTENTIOMETER
  if (HAS_POTENTIOMETER) {
    const char* potStr = "?";
    switch (systemStatus.potentiometer) {
      case INIT_NOT_STARTED: potStr = "Non demarre"; break;
      case INIT_IN_PROGRESS: potStr = "En cours"; break;
      case INIT_SUCCESS:
        LogManager::info("[INIT] Potentiometre: OK");
        LogManager::info("[INIT]   -> Valeur: %d%%", PotentiometerManager::getLastValue());
        potStr = nullptr;
        break;
      case INIT_FAILED: potStr = "Non disponible"; break;
    }
    if (potStr != nullptr) {
      LogManager::info("[INIT] Potentiometre: %s", potStr);
    }
  }
  #endif
  
  #ifdef HAS_AUDIO
  if (HAS_AUDIO) {
    const char* audioStr = "?";
    switch (systemStatus.audio) {
      case INIT_NOT_STARTED: audioStr = "Non demarre"; break;
      case INIT_IN_PROGRESS: audioStr = "En cours"; break;
      case INIT_SUCCESS:
        LogManager::info("[INIT] Audio: OK");
        LogManager::info("[INIT]   -> Volume: %d/21", AudioManager::getVolume());
        audioStr = nullptr;
        break;
      case INIT_FAILED: audioStr = "Non disponible"; break;
    }
    if (audioStr != nullptr) {
      LogManager::info("[INIT] Audio: %s", audioStr);
    }
  }
  #endif
  
  #ifdef HAS_VIBRATOR
  if (HAS_VIBRATOR) {
    const char* vibStr = "?";
    switch (systemStatus.vibrator) {
      case INIT_NOT_STARTED: vibStr = "Non demarre"; break;
      case INIT_IN_PROGRESS: vibStr = "En cours"; break;
      case INIT_SUCCESS: vibStr = "OK"; break;
      case INIT_FAILED: vibStr = "Non disponible"; break;
    }
    LogManager::info("[INIT] Vibrator: %s", vibStr);
  }
  #endif
  
  #ifdef HAS_TOUCH
  if (HAS_TOUCH) {
    const char* touchStr = "?";
    switch (systemStatus.touch) {
      case INIT_NOT_STARTED: touchStr = "Non demarre"; break;
      case INIT_IN_PROGRESS: touchStr = "En cours"; break;
      case INIT_SUCCESS: touchStr = "OK"; break;
      case INIT_FAILED: touchStr = "Non disponible"; break;
    }
    LogManager::info("[INIT] Touch (TTP223): %s", touchStr);
  }
  #endif
  
  #ifdef HAS_ENV_SENSOR
  if (HAS_ENV_SENSOR) {
    const char* envStr = "?";
    switch (systemStatus.envSensor) {
      case INIT_NOT_STARTED: envStr = "Non demarre"; break;
      case INIT_IN_PROGRESS: envStr = "En cours"; break;
      case INIT_SUCCESS: envStr = "OK"; break;
      case INIT_FAILED: envStr = "Non disponible"; break;
    }
    LogManager::info("[INIT] Env Sensor (AHT20+BMP280): %s", envStr);
  }
  #endif
  
  LogManager::info("[INIT] Systeme pret: %s", isSystemReady() ? "OUI" : "NON");
  LogManager::info("[INIT] ========================================");
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
