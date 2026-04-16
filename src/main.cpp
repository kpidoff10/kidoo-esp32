#include <Arduino.h>
#include <esp_task_wdt.h>
#include "common/managers/init/init_manager.h"
#include "common/managers/serial/serial_commands.h"
#include "common/managers/mqtt/mqtt_manager.h"
#include "common/managers/ota/ota_manager.h"
#include "common/managers/potentiometer/potentiometer_manager.h"
#include "models/model_config.h"
#include "models/model_init.h"
#include "common/config/core_config.h"

#ifdef HAS_WIFI
#include "common/managers/wifi/wifi_manager.h"
#endif

#ifdef HAS_BLE
#include "common/managers/ble_config/ble_config_manager.h"
#endif

// RTC pour synchronisation automatique lors de la connexion WiFi
#ifdef HAS_RTC
#include "common/managers/rtc/rtc_manager.h"
#endif

#ifdef HAS_TOUCH
#include "common/managers/touch/touch_manager.h"
#endif

#ifdef HAS_LCD
#include "common/managers/lcd/lcd_manager.h"
#endif

/**
 * Architecture multi-cœurs ESP32 (auto-détectée)
 * ==============================================
 * 
 * La configuration s'adapte automatiquement au type de chip :
 * 
 * ESP32-S3 (Basic) : Dual-core + PSRAM
 * - Core 0 : WiFi stack, MQTT, WiFi retry
 * - Core 1 : loop(), LEDManager, BLE
 * - PSRAM pour les buffers
 * 
 * ESP32-C3 (Mini) : Single-core RISC-V
 * - Core 0 : Tout (seul cœur disponible)
 * - Priorités ajustées pour éviter les conflits
 * - Pas de PSRAM
 * 
 * Voir core_config.h pour la configuration complète.
 */

void setup() {
  // Arduino v3.x: WiFiClientSecure spam esp_task_wdt_reset. Désactiver le WDT.
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_task_wdt_deinit();
#endif
  // Forcer la fréquence CPU maximale pour de meilleures performances
  // ESP32-S3 : 240MHz, ESP32-C3 : 160MHz max
  #if IS_SINGLE_CORE
  setCpuFrequencyMhz(160);  // ESP32-C3 max = 160MHz
  #else
  setCpuFrequencyMhz(240);  // ESP32-S3 max = 240MHz
  #endif
  
  // Afficher les informations seulement si Serial est disponible
  if (Serial) {
    Serial.print("[MAIN] CPU Frequency: ");
    Serial.print(getCpuFrequencyMhz());
    Serial.println(" MHz");
    
    // Afficher l'architecture CPU détectée (depuis core_config.h)
    printCoreArchitecture();
    
    // Afficher les statistiques mémoire (depuis core_config.h)
    printMemoryStats();
  }
  
  // Initialiser tous les composants du système via le gestionnaire d'initialisation
  if (!InitManager::init()) {
    if (Serial) {
      Serial.println("[MAIN] ERREUR: Echec de l'initialisation du systeme");
    }
    // Le système peut continuer avec des composants partiellement initialisés
  }
  
  // Initialiser le système de commandes Serial seulement si Serial est disponible
  if (Serial) {
    SerialCommands::init();
    
    // Afficher le statut final
    InitManager::printStatus();
  }
}

void loop() {
  // ====================================================================
  // loop() Arduino - Core dépend du chip
  // ====================================================================
  // - ESP32-S3 (Basic) : Core 1 (APP_CPU)
  // - ESP32-C3 (Mini)  : Core 0 (seul cœur)
  // Les threads FreeRTOS gèrent les tâches temps-réel indépendamment.
  // ====================================================================

#ifdef HAS_LCD
  // Ré-init LCD ~2,5 s après boot (corrige "après reboot pas d'affichage", upload OK)
  LCDManager::tryDelayedReinit();
#endif
  
  // Traiter les commandes Serial en attente
  SerialCommands::update();

  // Mettre à jour le touch (TTP223) en début de loop pour que tout le reste voie un état à jour
  #ifdef HAS_TOUCH
  if (HAS_TOUCH) {
    TouchManager::update();
  }
  #endif
  
  #ifdef HAS_MQTT
  // Note: MqttManager::loop() ne fait plus rien - le thread gère tout
  // On garde l'appel pour compatibilité mais il est vide
  MqttManager::loop();

  // Retry périodique publication statut OTA (firmware-update-done/failed) pendant 60 s au boot
  // Au cas où l'appel en init échoue (MQTT pas encore prêt), on réessaie
  static unsigned long lastOtaPublishRetry = 0;
  if (millis() < 60000 && (millis() - lastOtaPublishRetry > 3000)) {
    lastOtaPublishRetry = millis();
    OTAManager::publishLastOtaErrorIfAny();
  }
  
  // Vérifier si MQTT doit se connecter automatiquement quand le WiFi devient disponible
  // (si MQTT est initialisé mais pas connecté, et que le WiFi est maintenant connecté)
  // Ne pas tenter pendant OTA : on a libéré MQTT volontairement, on le reconnecte nous-mêmes en cas d'échec
  static unsigned long lastMQTTConnectAttempt = 0;
  if (MqttManager::isInitialized() && !MqttManager::isConnected()
      && !OTAManager::isOtaInProgress()) {
    #ifdef HAS_WIFI
    if (WiFiManager::isConnected() && (millis() - lastMQTTConnectAttempt > 5000)) {
      lastMQTTConnectAttempt = millis();
      MqttManager::connect();
    }
    #endif
  }
  #endif

  // Synchroniser l'heure RTC régulièrement tant qu'elle n'est pas valide
  // Retry toutes les secondes si le WiFi est connecté
  #ifdef HAS_RTC
  static unsigned long lastRtcSyncAttempt = 0;
  if (WiFiManager::isConnected() && !RTCManager::isTimeValid()
      && (millis() - lastRtcSyncAttempt > 1000)) {
    lastRtcSyncAttempt = millis();
    if (Serial) {
      Serial.println("[MAIN] RTC - Tentative synchronisation NTP...");
    }
    RTCManager::autoSyncIfNeeded();
  }
  #endif

  // Gérer les initialisations lazy quand le WiFi se connecte
  #ifdef HAS_WIFI
  static bool lastWiFiState = false;
  if (WiFiManager::isConnected() && !lastWiFiState) {
    // WiFi vient de se connecter

    // Synchroniser l'heure RTC via NTP
    #ifdef HAS_RTC
    RTCManager::autoSyncIfNeeded();
    #endif

    // Initialiser MQTT (lazy init) — sauf pendant le setup BLE
    // Le MQTT sera initialisé après la commande BLE "registered"
    #ifdef HAS_MQTT
    if (HAS_MQTT && !MqttManager::isInitialized() && !WiFiManager::isSkipPostConnectActions()) {
      MqttManager::init();
    }
    #endif
  }
  lastWiFiState = WiFiManager::isConnected();
  #endif
  
  // Mettre à jour le potentiomètre (détection de changement)
  #ifdef HAS_POTENTIOMETER
  PotentiometerManager::update();
  #endif
  
  // Mettre à jour le gestionnaire BLE Config (détection appui bouton)
  // BLE s'active seulement sur appui long bouton (3 secondes)
  #ifdef HAS_BLE
  if (HAS_BLE && BLEConfigManager::isInitialized()) {
    #ifdef BLE_CONFIG_BUTTON_PIN
    BLEConfigManager::update();
    #endif
  }
  #endif
  
  // Mise à jour du modèle (Dream : bedtime, wakeup, touch | Gotchi : UI AMOLED, etc. | Sound : audio)
  InitModel::update();
  
  // ====================================================================
  // Threads indépendants (gérés par FreeRTOS, ne pas appeler ici) :
  // - LEDManager   : CORE_LED, PRIORITY_LED, animations temps-réel
  // - AudioManager : CORE_AUDIO, PRIORITY_AUDIO, lecture I2S temps-réel
  // - MqttManager: CORE_mqtt, PRIORITY_mqtt, HTTP polling
  // - WiFi retry   : CORE_WIFI_RETRY, PRIORITY_WIFI_RETRY, reconnexion
  // (voir core_config.h pour les valeurs selon le chip)
  // ====================================================================
  
  // Petite pause pour éviter de surcharger le CPU
  delay(10);
}