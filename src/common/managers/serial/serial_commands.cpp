#include "serial_commands.h"
#include "serial_manager.h"
#include "common/managers/led/led_manager.h"
#include "common/managers/init/init_manager.h"
#include "common/managers/sd/sd_manager.h"
#include <SD.h>
#include <ArduinoJson.h>
#include "common/managers/ble/ble_manager.h"
#ifdef HAS_BLE
#include "common/managers/ble_config/ble_config_manager.h"
#endif
#include "common/managers/wifi/wifi_manager.h"
#ifdef HAS_WIFI
#include <WiFi.h>
#include <HTTPClient.h>
#include "app_config.h"
#include "ssl_config.h"
#endif
#ifdef HAS_MQTT
#include "common/managers/mqtt/mqtt_manager.h"
#endif
#include "common/managers/rtc/rtc_manager.h"
#include "common/managers/potentiometer/potentiometer_manager.h"
#include "common/managers/nfc/nfc_manager.h"
#include "common/managers/ota/ota_manager.h"
#ifdef HAS_AUDIO
#include "common/managers/audio/audio_manager.h"
#endif
#ifdef HAS_LCD
#include "common/managers/lcd/lcd_manager.h"
#endif
#ifdef HAS_VIBRATOR
#include "common/managers/vibrator/vibrator_manager.h"
#endif
#ifdef HAS_TOUCH
#include "common/managers/touch/touch_manager.h"
#endif
#ifdef HAS_ENV_SENSOR
#include "common/managers/env_sensor/env_sensor_manager.h"
#endif
#ifdef HAS_SD
#include "common/managers/device_key/device_key_manager.h"
#endif
#include "models/model_serial_commands.h"
#ifdef KIDOO_MODEL_DREAM
#include "models/dream/config_sync/model_config_sync_routes.h"
#endif
#ifdef HAS_MQTT
#include "models/model_mqtt_routes.h"
#endif
#include "models/model_config.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Variables statiques
bool SerialCommands::initialized = false;
String SerialCommands::inputBuffer = "";

// Historique des commandes
String SerialCommands::history[SerialCommands::HISTORY_MAX];
uint8_t SerialCommands::historyCount = 0;
int8_t  SerialCommands::historyIndex = -1;
String  SerialCommands::tempBuffer = "";
uint8_t SerialCommands::escState = 0;

void SerialCommands::init() {
  if (initialized) {
    return;
  }
  
  initialized = true;
  inputBuffer = "";
  
  // Initialiser seulement si Serial est disponible (USB connecté)
}

void SerialCommands::replaceInputBuffer(const String& newContent) {
  // Effacer les caractères actuels avec backspace
  for (int i = inputBuffer.length(); i > 0; i--) {
    Serial.print("\b \b");
  }
  inputBuffer = newContent;
  Serial.print(inputBuffer);
}

void SerialCommands::update() {
  // Vérifier que Serial est disponible avant d'essayer de lire
  if (!Serial || !Serial.available()) {
    return;
  }

  // Lire les caractères disponibles
  while (Serial.available()) {
    char c = Serial.read();

    // Machine d'états pour les séquences ESC (touches flèches)
    if (escState == 1) {
      if (c == '[') { escState = 2; continue; }
      escState = 0;
    } else if (escState == 2) {
      escState = 0;
      if (c == 'A') {
        // Flèche haut — commande précédente
        if (historyCount > 0) {
          if (historyIndex == -1) {
            tempBuffer = inputBuffer;
            historyIndex = (int8_t)(historyCount - 1);
          } else if (historyIndex > 0) {
            historyIndex--;
          }
          replaceInputBuffer(history[historyIndex]);
        }
        continue;
      } else if (c == 'B') {
        // Flèche bas — commande suivante
        if (historyIndex != -1) {
          if (historyIndex < (int8_t)(historyCount - 1)) {
            historyIndex++;
            replaceInputBuffer(history[historyIndex]);
          } else {
            historyIndex = -1;
            replaceInputBuffer(tempBuffer);
          }
        }
        continue;
      }
      continue;
    }

    // Détecter début de séquence ESC
    if (c == 0x1B) { escState = 1; continue; }

    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() > 0) {
        processCommand(inputBuffer);
        inputBuffer = "";
      }
      historyIndex = -1;
      tempBuffer = "";
    } else if (c == '\b' || c == 127) {
      // Backspace
      if (inputBuffer.length() > 0) {
        inputBuffer.remove(inputBuffer.length() - 1);
        Serial.print("\b \b");
      }
      // Réinitialiser l'historique lors de l'édition
      if (historyIndex != -1) {
        historyIndex = -1;
        tempBuffer = "";
      }
    } else if (c >= 32 && c <= 126) {
      // Caractère imprimable
      inputBuffer += c;
      Serial.print(c);
      // Réinitialiser l'historique lors de l'édition
      if (historyIndex != -1) {
        historyIndex = -1;
        tempBuffer = "";
      }
    }
  }
}

void SerialCommands::processCommand(const String& command) {
  if (command.length() == 0) {
    return;
  }

  // Ajouter à l'historique (si différent de la dernière commande)
  if (historyCount == 0 || history[historyCount - 1] != command) {
    if (historyCount < HISTORY_MAX) {
      history[historyCount++] = command;
    } else {
      // Décaler pour faire de la place (FIFO)
      for (uint8_t i = 0; i < HISTORY_MAX - 1; i++) {
        history[i] = history[i + 1];
      }
      history[HISTORY_MAX - 1] = command;
    }
  }

  // Réinitialiser l'index d'historique
  historyIndex = -1;
  tempBuffer = "";

  Serial.println(); // Nouvelle ligne après la commande

  // Séparer la commande et les arguments
  int spaceIndex = command.indexOf(' ');
  String cmd = command;
  String args = "";
  
  if (spaceIndex > 0) {
    cmd = command.substring(0, spaceIndex);
    args = command.substring(spaceIndex + 1);
  }
  
  cmd.toLowerCase();
  cmd.trim();
  args.trim();
  
  // Traiter les commandes communes
  if (cmd == "help" || cmd == "?") {
    cmdHelp();
  } else if (cmd == "reboot" || cmd == "restart") {
    cmdReboot(args);
  } else if (cmd == "info" || cmd == "system") {
    cmdInfo();
  } else if (cmd == "memory" || cmd == "mem") {
    cmdMemory();
  } else if (cmd == "clear" || cmd == "cls") {
    cmdClear();
  } else if (cmd == "brightness" || cmd == "bright") {
    cmdBrightness(args);
  } else if (cmd == "sleep" || cmd == "sleepmode") {
    cmdSleep(args);
  } else if (cmd == "ble" || cmd == "bluetooth" || cmd == "ble-status") {
    cmdBLE();
  } else if (cmd == "ble-start" || cmd == "ble-pair" || cmd == "ble-appairer") {
    cmdBLEStart();
  } else if (cmd == "ble-stop") {
    cmdBLEStop();
  } else if (cmd == "wifi" || cmd == "wifi-status") {
    cmdWiFi();
  } else if (cmd == "wifi-set") {
    cmdWiFiSet(args);
  } else if (cmd == "wifi-connect") {
    cmdWiFiConnect();
  } else if (cmd == "wifi-disconnect") {
    cmdWiFiDisconnect();
  } else if (cmd == "wifi-scan" || cmd == "scan-wifi") {
    cmdWiFiScan();
  } else if (cmd == "config-retry" || cmd == "retry-config") {
    cmdConfigRetry();
  } else if (cmd == "device-key" || cmd == "public-key" || cmd == "cle-device") {
    cmdDeviceKey();
  #ifdef HAS_MQTT
  } else if (cmd == "mqtt" || cmd == "mqtt-status") {
    cmdMqtt();
  } else if (cmd == "mqtt-connect") {
    cmdMqttConnect();
  } else if (cmd == "mqtt-disconnect") {
    cmdMqttDisconnect();
  } else if (cmd == "mqtt-publish" || cmd == "mqtt-pub") {
    cmdMqttPublish(args);
  } else if (cmd == "mqtt-routes" || cmd == "routes") {
    cmdMqttRoutes();
  #endif
  } else if (cmd == "rtc" || cmd == "time" || cmd == "date") {
    cmdRTC();
  } else if (cmd == "rtc-set" || cmd == "time-set") {
    cmdRTCSet(args);
  } else if (cmd == "rtc-sync" || cmd == "ntp" || cmd == "ntp-sync") {
    cmdRTCSync();
  } else if (cmd == "tz" || cmd == "timezone" || cmd == "timezone-show") {
    cmdTimezone();
  } else if (cmd == "api-ping" || cmd == "ping-api" || cmd == "ping-server") {
    cmdApiPing();
  } else if (cmd == "pot" || cmd == "potentiometer" || cmd == "volume") {
    cmdPotentiometer();
  } else if (cmd == "memdebug" || cmd == "mem-debug" || cmd == "raminfo") {
    cmdMemoryDebug();
  } else if (cmd == "nfc-read" || cmd == "nfc-read-uid") {
    cmdNFCRead(args);
  } else if (cmd == "nfc-write" || cmd == "nfc-write-block") {
    cmdNFCWrite(args);
  } else if (cmd == "config-get" || cmd == "cfg-get") {
    cmdConfigGet(args);
  } else if (cmd == "config-set" || cmd == "cfg-set") {
    cmdConfigSet(args);
  } else if (cmd == "config-list" || cmd == "cfg-list" || cmd == "config") {
    cmdConfigList();
  } else if (cmd == "ota" || cmd == "ota-update" || cmd == "update") {
    cmdOta(args);
  #ifdef HAS_VIBRATOR
  } else if (cmd == "vibrator" || cmd == "vibe" || cmd == "vib") {
    cmdVibrator(args);
  #endif
  #ifdef HAS_TOUCH
  } else if (cmd == "touch" || cmd == "tap") {
    cmdTouch(args);
  #endif
  #ifdef HAS_ENV_SENSOR
  } else if (cmd == "env" || cmd == "env-status" || cmd == "env-read" || cmd == "temperature" || cmd == "humidity") {
    cmdEnv(args);
  #endif
  #ifdef HAS_LED
  } else if (cmd == "led-test" || cmd == "test-led" || cmd == "testleds") {
    cmdLEDTest();
  #endif
  #ifdef HAS_LCD
  } else if (cmd == "lcd-test" || cmd == "test-lcd" || cmd == "testlcd") {
    cmdLCDTest();
  } else if (cmd == "lcd-reset" || cmd == "lcd-reset-display") {
    cmdLCDReset();
  } else if (cmd == "lcd-fps" || cmd == "fps" || cmd == "lcd-fps-test") {
    cmdLCDFps();
  } else if (cmd == "lcd-play-mjpeg" || cmd == "lcd-play-ffmpeg" || cmd == "video-play" || cmd == "play-mjpeg") {
    cmdLCDPlayMjpeg(args);
  #endif
  #ifdef HAS_AUDIO
  } else if (cmd == "audio" || cmd == "audio-status") {
    cmdAudio();
  } else if (cmd == "play" || cmd == "audio-play") {
    cmdAudioPlay(args);
  } else if (cmd == "stop" || cmd == "audio-stop") {
    cmdAudioStop();
  } else if (cmd == "pause" || cmd == "audio-pause") {
    cmdAudioPause();
  } else if (cmd == "resume" || cmd == "audio-resume") {
    cmdAudioResume();
  } else if (cmd == "vol" || cmd == "audio-vol" || cmd == "audio-volume") {
    cmdAudioVolume(args);
  } else if (cmd == "ls" || cmd == "audio-list" || cmd == "list") {
    cmdAudioList(args);
  #endif
  } else {
    // Essayer les commandes spécifiques au modèle
    if (!ModelSerialCommands::processCommand(command)) {
      // Commande non reconnue
      Serial.print("[SERIAL] Commande inconnue: ");
      Serial.println(cmd);
      Serial.println("[SERIAL] Tapez 'help' pour voir les commandes disponibles");
    }
  }
}

void SerialCommands::printHelp() {
  Serial.println("");
  Serial.println("========================================");
  Serial.println("     COMMANDES SERIAL DISPONIBLES");
  Serial.println("========================================");
  Serial.println("  help, ?          - Afficher cette aide");
  Serial.println("  reboot [ms]      - Redemarrer l'ESP32 (optionnel: delai en ms)");
  Serial.println("  info, system     - Afficher les informations systeme");
  Serial.println("  memory, mem      - Afficher l'utilisation de la memoire");
  Serial.println("  clear, cls       - Effacer l'ecran");
  Serial.println("  memdebug, raminfo - Analyse detaillee de la RAM par composant");
  
  #ifdef HAS_LED
  if (HAS_LED) {
    Serial.println("  brightness [%]   - Afficher ou definir la luminosite (0-100%)");
    Serial.println("  sleep [timeout]  - Afficher ou definir le timeout sleep mode (ms, min: 5000, 0=desactive)");
    Serial.println("  led-test         - Tester les LEDs une par une puis toutes en rouge");
  }
  #endif
  #ifdef HAS_LCD
  if (HAS_LCD) {
    Serial.println("  lcd-test         - Tester l'ecran LCD (rouge, bleu, vert)");
    Serial.println("  lcd-reset        - Reinitialiser l'ecran (si plus de reponse)");
    Serial.println("  lcd-fps          - Animation frame par frame, mesurer les FPS");
    Serial.println("  lcd-play-mjpeg [p] - Jouer un .mjpeg video depuis la SD (defaut: /video.mjpeg)");
  }
  #endif
  
  #ifdef HAS_BLE
  if (HAS_BLE) {
    Serial.println("  ble, bluetooth   - Afficher l'etat de connexion BLE");
    Serial.println("  ble-start        - Lancer l'appareillage BLE (visible pour l'app mobile)");
    Serial.println("  ble-stop         - Arreter le mode appareillage BLE");
    Serial.println("  (ble-pair / ble-appairer = alias de ble-start)");
  }
  #endif
  
  #ifdef HAS_WIFI
  if (HAS_WIFI) {
    Serial.println("  wifi             - Afficher l'etat de connexion WiFi");
    Serial.println("  wifi-set <ssid> [password] - Configurer le WiFi");
    Serial.println("  wifi-connect     - Se connecter au WiFi configure");
    Serial.println("  wifi-disconnect  - Se deconnecter du WiFi");
    Serial.println("  wifi-scan        - Scanner les reseaux WiFi disponibles");
    Serial.println("  api-ping         - Tester la connectivite au serveur API");
    Serial.println("  ota <version>    - Mise a jour OTA vers la version (ex: ota 1.0.26)");
  }
  #endif
  
  #ifdef HAS_MQTT
  if (HAS_MQTT) {
    Serial.println("  mqtt             - Afficher l'etat MQTT");
    Serial.println("  mqtt-connect   - Se connecter a MQTT");
    Serial.println("  mqtt-disconnect - Se deconnecter de MQTT");
    Serial.println("  mqtt-pub <msg> - Publier un message");
    Serial.println("  mqtt-routes    - Afficher les routes MQTT disponibles");
  }
  #endif
  
  #ifdef HAS_RTC
  if (HAS_RTC) {
    Serial.println("  rtc, time, date  - Afficher l'heure et la date du RTC");
    Serial.println("  rtc-set <timestamp|DD/MM/YYYY HH:MM:SS> - Definir l'heure");
    Serial.println("  rtc-sync, ntp    - Synchroniser l'heure via NTP (WiFi requis)");
    Serial.println("  tz, timezone     - Afficher le fuseau horaire actuel");
  }
  #endif
  
  #ifdef HAS_POTENTIOMETER
  if (HAS_POTENTIOMETER) {
    Serial.println("  pot, volume      - Afficher la valeur du potentiometre");
  }
  #endif
  
  #ifdef HAS_NFC
  if (HAS_NFC) {
    Serial.println("  nfc-read [block] - Lire l'UID d'un tag NFC (optionnel: lire un bloc)");
    Serial.println("  nfc-write <block> <data> - Ecrire des donnees sur un tag NFC");
  }
  #endif
  
  #ifdef HAS_VIBRATOR
  if (HAS_VIBRATOR) {
    Serial.println("  vibrator [on|off|toggle|test|status] - Controle du vibreur");
    Serial.println("  vibrator intensity [0-255] - Intensite PWM");
    Serial.println("  vibrator short|long|saccade|pulse|doubletap - Effets (court, long, saccade, pulsation, double tap)");
  }
  #endif
  
  #ifdef HAS_TOUCH
  if (HAS_TOUCH) {
    Serial.println("  touch [status|read] - Capteur tactile TTP223 (etat debounce ou lecture brute)");
  }
  #endif
  
  #ifdef HAS_ENV_SENSOR
  if (HAS_ENV_SENSOR) {
    Serial.println("  env [status|read]  - Capteur AHT20+BMP280 (temperature, humidite, pression)");
  }
  #endif
  
  Serial.println("  config-list, config - Afficher toutes les cles de config.json");
  Serial.println("  config-get <key>   - Lire une cle de config.json");
  Serial.println("  config-set <key> <value> - Definir une cle dans config.json");
  Serial.println("  config-retry      - Tester retry sync config (avec signature RTC, Dream)");
  #ifdef HAS_SD
  Serial.println("  device-key        - Afficher la cle publique Ed25519 (auth device)");
  #endif
  
  #ifdef HAS_AUDIO
  if (HAS_AUDIO) {
    Serial.println("");
    Serial.println("--- Commandes Audio ---");
    Serial.println("  audio              - Afficher le statut audio");
    Serial.println("  play <fichier>     - Lire un fichier audio (ex: play /music/song.mp3)");
    Serial.println("  stop               - Arreter la lecture");
    Serial.println("  pause              - Mettre en pause");
    Serial.println("  resume             - Reprendre la lecture");
    Serial.println("  vol [0-100]        - Afficher ou definir le volume (%)");
    Serial.println("  ls [dossier]       - Lister les fichiers audio (ex: ls /music)");
  }
  #endif
  
  Serial.println("========================================");
  
  // Afficher l'aide des commandes spécifiques au modèle
  ModelSerialCommands::printHelp();
}

void SerialCommands::cmdHelp() {
  printHelp();
}

void SerialCommands::cmdReboot(const String& args) {
  uint32_t delayMs = 0;
  
  if (args.length() > 0) {
    delayMs = args.toInt();
  }
  
  SerialManager::reboot(delayMs);
}

void SerialCommands::cmdInfo() {
  SerialManager::printSystemInfo();
}

void SerialCommands::cmdMemory() {
  SerialManager::printMemoryInfo();
}

void SerialCommands::cmdClear() {
  // Effacer l'écran (séquence ANSI)
  Serial.print("\033[2J");  // Effacer l'écran
  Serial.print("\033[H");   // Curseur en haut à gauche
}

void SerialCommands::cmdBrightness(const String& args) {
  if (!LEDManager::isInitialized()) {
    Serial.println("[SERIAL] LED Manager non initialise");
    return;
  }
  
  if (args.length() == 0) {
    // Afficher la luminosité actuelle en %
    uint8_t brightness = LEDManager::getCurrentBrightness();
    uint8_t percent = (brightness * 100) / 255;
    Serial.print("[SERIAL] Luminosite actuelle: ");
    Serial.print(percent);
    Serial.println("%");
  } else {
    // Définir la luminosité en %
    int percent = args.toInt();
    
    if (percent < 0 || percent > 100) {
      Serial.println("[SERIAL] Erreur: La luminosite doit etre entre 0 et 100%");
      return;
    }
    
    // Convertir % en valeur brute (0-255) avec arrondi correct
    uint8_t brightness = LEDManager::brightnessPercentTo255((uint8_t)percent);
    
    // Réveiller les LEDs pour désactiver le sleep mode lors des tests
    LEDManager::wakeUp();
    
    if (LEDManager::setBrightness(brightness)) {
      // Mettre à jour et sauvegarder la configuration
      SDConfig config = InitManager::getConfig();
      config.led_brightness = brightness;
      
      if (SDManager::isAvailable() && InitManager::updateConfig(config)) {
        Serial.print("[SERIAL] Luminosite definie a: ");
        Serial.print(percent);
        Serial.println("% (sauvegarde dans config.json)");
      } else {
        Serial.print("[SERIAL] Luminosite definie a: ");
        Serial.print(percent);
        Serial.println("% (sauvegarde echec)");
      }
    } else {
      Serial.println("[SERIAL] Erreur: Impossible de definir la luminosite");
    }
  }
}

void SerialCommands::cmdSleep(const String& args) {
  if (!LEDManager::isInitialized()) {
    Serial.println("[SERIAL] LED Manager non initialise");
    return;
  }
  
  // Pour le sleep mode, on doit accéder à la config via InitManager
  // car le timeout est chargé depuis la config au démarrage
  // Pour l'instant, on peut juste consulter via la config
  // et modifier en runtime sans sauvegarder
  
  if (args.length() == 0) {
    // Afficher le timeout actuel du sleep mode
    const SDConfig& config = InitManager::getConfig();
    uint32_t timeout = config.sleep_timeout_ms;
    
    Serial.print("[SERIAL] Sleep mode timeout: ");
    if (timeout == 0) {
      Serial.println("Desactive");
    } else {
      Serial.print(timeout);
      Serial.print(" ms (");
      Serial.print(timeout / 1000.0f, 1);
      Serial.println(" s)");
    }
    
    bool isSleeping = LEDManager::getSleepState();
    Serial.print("[SERIAL] Sleep mode actuel: ");
    Serial.println(isSleeping ? "Actif (LEDs eteintes)" : "Inactif (LEDs actives)");
  } else {
    // Définir le timeout du sleep mode
    // Note: Ceci modifie seulement en runtime, pas dans la config
    int timeout = args.toInt();
    
    if (timeout < 0) {
      Serial.println("[SERIAL] Erreur: Le timeout ne peut pas etre negatif");
      return;
    }
    
    if (timeout > 0 && timeout < 5000) {
      Serial.println("[SERIAL] Erreur: Le timeout minimum est de 5000 ms (5 secondes)");
      Serial.println("[SERIAL] Utilisez 0 pour desactiver le sleep mode");
      return;
    }
    
    // Mettre à jour le sleep timeout dans la configuration
    SDConfig config = InitManager::getConfig();
    config.sleep_timeout_ms = (uint32_t)timeout;
    
    if (SDManager::isAvailable() && InitManager::updateConfig(config)) {
      // Mettre à jour le timeout dans LEDManager
      // Note: LEDManager lit sleepTimeoutMs depuis InitManager::getConfig() au démarrage
      // Pour le runtime, on devrait ajouter une méthode setSleepTimeout() dans LEDManager
      // Pour l'instant, il faudra redémarrer pour que le changement prenne effet
      Serial.print("[SERIAL] Sleep timeout defini a: ");
      if (timeout == 0) {
        Serial.println("Desactive (sauvegarde dans config.json)");
      } else {
        Serial.print(timeout);
        Serial.println(" ms (sauvegarde dans config.json)");
        Serial.println("[SERIAL] Note: Redemarrez pour appliquer le nouveau timeout");
      }
    } else {
      Serial.println("[SERIAL] Erreur: Impossible de sauvegarder le sleep timeout");
    }
  }
}

void SerialCommands::cmdBLE() {
  Serial.println("");
  Serial.println("========== Etat BLE ==========");
  
#ifndef HAS_BLE
  Serial.println("[BLE] BLE non disponible sur ce modele");
  Serial.println("==============================");
  return;
#else
  // Vérifier l'initialisation
  if (!BLEManager::isInitialized()) {
    Serial.println("[BLE] BLE non initialise");
    Serial.println("==============================");
    return;
  }
  
  // Vérifier la disponibilité
  if (!BLEManager::isAvailable()) {
    Serial.println("[BLE] BLE non disponible");
    Serial.println("==============================");
    return;
  }
  
  // Afficher l'état de connexion
  bool connected = BLEManager::isConnected();
  Serial.print("[BLE] Connexion: ");
  if (connected) {
    Serial.println("CONNECTE");
  } else {
    Serial.println("NON CONNECTE");
  }
  
  // Afficher le statut depuis InitManager
  InitStatus bleStatus = InitManager::getComponentStatus("ble");
  Serial.print("[BLE] Statut initialisation: ");
  switch (bleStatus) {
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
  
  Serial.println("==============================");
#endif
}

void SerialCommands::cmdBLEStart() {
#ifndef HAS_BLE
  Serial.println("[BLE] BLE non disponible sur ce modele");
  return;
#else
  Serial.println("[BLE] Lancement de l'appareillage BLE...");
  if (BLEConfigManager::enableBLE(0, true)) {
    Serial.println("[BLE] BLE active. L'appareil est visible pour l'appairage (duree par defaut: 15 min).");
  } else {
    Serial.println("[BLE] Echec activation BLE");
  }
#endif
}

void SerialCommands::cmdBLEStop() {
#ifndef HAS_BLE
  Serial.println("[BLE] BLE non disponible sur ce modele");
  return;
#else
  Serial.println("[BLE] Arret du mode appareillage BLE.");
  BLEConfigManager::disableBLE();
  Serial.println("[BLE] BLE desactive.");
#endif
}

void SerialCommands::cmdWiFi() {
  WiFiManager::printInfo();
}

void SerialCommands::cmdWiFiSet(const String& args) {
#ifndef HAS_WIFI
  Serial.println("[WIFI] WiFi non disponible sur ce modele");
  return;
#else
  if (args.length() == 0) {
    Serial.println("[WIFI] Usage: wifi-set <ssid> [password]");
    Serial.println("[WIFI] Exemple: wifi-set MonReseau MonMotDePasse");
    Serial.println("[WIFI] Note: Si pas de mot de passe, laissez vide");
    return;
  }
  
  // Séparer SSID et mot de passe
  String ssid = "";
  String password = "";
  
  int spaceIndex = args.indexOf(' ');
  if (spaceIndex > 0) {
    ssid = args.substring(0, spaceIndex);
    password = args.substring(spaceIndex + 1);
    password.trim();
  } else {
    ssid = args;
  }
  
  ssid.trim();
  
  if (ssid.length() == 0) {
    Serial.println("[WIFI] Erreur: SSID invalide");
    return;
  }
  
  if (ssid.length() >= 64) {
    Serial.println("[WIFI] Erreur: SSID trop long (max 63 caracteres)");
    return;
  }
  
  if (password.length() >= 64) {
    Serial.println("[WIFI] Erreur: Mot de passe trop long (max 63 caracteres)");
    return;
  }
  
  // Mettre à jour la configuration
  SDConfig config = InitManager::getConfig();
  strncpy(config.wifi_ssid, ssid.c_str(), sizeof(config.wifi_ssid) - 1);
  config.wifi_ssid[sizeof(config.wifi_ssid) - 1] = '\0';
  strncpy(config.wifi_password, password.c_str(), sizeof(config.wifi_password) - 1);
  config.wifi_password[sizeof(config.wifi_password) - 1] = '\0';
  
  // Sauvegarder
  if (SDManager::isAvailable() && InitManager::updateConfig(config)) {
    Serial.println("[WIFI] Configuration WiFi sauvegardee:");
    Serial.print("[WIFI]   SSID: ");
    Serial.println(ssid);
    Serial.print("[WIFI]   Password: ");
    if (password.length() > 0) {
      Serial.println("********");
    } else {
      Serial.println("(aucun)");
    }
    Serial.println("[WIFI] Utilisez 'wifi-connect' pour vous connecter");
  } else {
    Serial.println("[WIFI] Erreur: Impossible de sauvegarder la configuration");
  }
#endif
}

void SerialCommands::cmdWiFiConnect() {
#ifndef HAS_WIFI
  Serial.println("[WIFI] WiFi non disponible sur ce modele");
  return;
#else
  if (!WiFiManager::isAvailable()) {
    Serial.println("[WIFI] WiFi non initialise");
    return;
  }
  
  // Vérifier si déjà connecté
  if (WiFiManager::isConnected()) {
    Serial.println("[WIFI] Deja connecte. Deconnexion...");
    WiFiManager::disconnect();
    delay(500);
  }
  
  // Se connecter avec la config
  Serial.println("[WIFI] Tentative de connexion...");
  if (WiFiManager::connect()) {
    Serial.println("[WIFI] Connexion reussie!");
  } else {
    Serial.println("[WIFI] Echec de connexion");
  }
#endif
}

void SerialCommands::cmdWiFiDisconnect() {
#ifndef HAS_WIFI
  Serial.println("[WIFI] WiFi non disponible sur ce modele");
  return;
#else
  if (!WiFiManager::isAvailable()) {
    Serial.println("[WIFI] WiFi non initialise");
    return;
  }
  
  if (!WiFiManager::isConnected()) {
    Serial.println("[WIFI] Pas connecte");
    return;
  }
  
  WiFiManager::disconnect();
  Serial.println("[WIFI] Deconnecte");
#endif
}

void SerialCommands::cmdWiFiScan() {
#ifdef HAS_WIFI
  if (!HAS_WIFI) {
    Serial.println("[WIFI] WiFi non disponible sur ce modele");
    return;
  }
  Serial.println("");
  Serial.println("========================================");
  Serial.println("          SCAN RESEAUX WIFI");
  Serial.println("========================================");

  int n = WiFi.scanNetworks();
  Serial.printf("Nombre de reseaux detectes: %d\n\n", n);

  if (n > 0) {
    Serial.println("Reseaux disponibles:");
    for (int i = 0; i < n && i < 20; i++) {
      Serial.print("  ");
      Serial.print(i + 1);
      Serial.print(". ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.println(" dBm)");
    }
    if (n > 20) Serial.printf("  ... et %d autres reseaux.\n", n - 20);
  } else {
    Serial.println("Aucun reseau WiFi detecte");
  }

  Serial.println("========================================");
  Serial.println("");
#else
  Serial.println("[WIFI] WiFi non disponible sur ce modele");
#endif
}

void SerialCommands::cmdConfigRetry() {
#ifdef HAS_WIFI
  if (!HAS_WIFI) {
    Serial.println("[CONFIG] WiFi non disponible");
    return;
  }
#ifdef KIDOO_MODEL_DREAM
  Serial.println("[CONFIG] Retry config-sync avec signature RTC...");
  ModelDreamConfigSyncRoutes::retryFetchConfig();
  Serial.println("[CONFIG] Retry lance");
#else
  Serial.println("[CONFIG] config-retry non disponible sur ce modele");
#endif
#else
  Serial.println("[CONFIG] WiFi non disponible");
#endif
}

void SerialCommands::cmdDeviceKey() {
#ifndef HAS_SD
  Serial.println("[DEVICE] Carte SD non disponible sur ce modele");
  return;
#else
  char pubKey[48] = {0};
  if (DeviceKeyManager::getOrCreatePublicKeyBase64(pubKey, sizeof(pubKey))) {
    Serial.println("[DEVICE] Cle publique Ed25519 (a mettre dans la DB si signature invalide):");
    Serial.println(pubKey);
  } else {
    Serial.println("[DEVICE] Cle device non disponible (SD?)");
  }
#endif
}

void SerialCommands::cmdMqtt() {
  MqttManager::printInfo();
}

void SerialCommands::cmdMqttConnect() {
#ifndef HAS_MQTT
  Serial.println("[MQTT] MQTT non disponible sur ce modele");
  return;
#else
  if (!MqttManager::isInitialized()) {
    // Tenter d'initialiser
    if (!MqttManager::init()) {
      Serial.println("[MQTT] Echec initialisation");
      return;
    }
  }
  
  // Vérifier si déjà connecté
  if (MqttManager::isConnected()) {
    Serial.println("[MQTT] Deja connecte");
    return;
  }
  
  // Se connecter
  Serial.println("[MQTT] Tentative de connexion...");
  if (MqttManager::connect()) {
    Serial.println("[MQTT] Connexion reussie!");
  } else {
    Serial.println("[MQTT] Echec de connexion");
  }
#endif
}

void SerialCommands::cmdMqttDisconnect() {
#ifndef HAS_MQTT
  Serial.println("[MQTT] MQTT non disponible sur ce modele");
  return;
#else
  if (!MqttManager::isConnected()) {
    Serial.println("[MQTT] Pas connecte");
    return;
  }
  
  MqttManager::disconnect();
  Serial.println("[MQTT] Deconnecte");
#endif
}

void SerialCommands::cmdMqttPublish(const String& args) {
#ifndef HAS_MQTT
  Serial.println("[MQTT] MQTT non disponible sur ce modele");
  return;
#else
  if (!MqttManager::isConnected()) {
    Serial.println("[MQTT] Non connecte");
    return;
  }
  
  if (args.length() == 0) {
    // Publier le statut par défaut
    if (MqttManager::publishStatus()) {
      Serial.println("[MQTT] Statut publie");
    } else {
      Serial.println("[MQTT] Echec publication");
    }
  } else {
    // Publier le message
    if (MqttManager::publish(args.c_str())) {
      Serial.print("[MQTT] Message publie: ");
      Serial.println(args);
    } else {
      Serial.println("[MQTT] Echec publication");
    }
  }
#endif
}

void SerialCommands::cmdMqttRoutes() {
#ifndef HAS_MQTT
  Serial.println("[MQTT] MQTT non disponible sur ce modele");
  return;
#else
  ModelMqttRoutes::printRoutes();
#endif
}

void SerialCommands::cmdRTC() {
#ifndef HAS_RTC
  Serial.println("[RTC] RTC non disponible sur ce modele");
  return;
#else
  RTCManager::printInfo();
#endif
}

void SerialCommands::cmdRTCSet(const String& args) {
#ifndef HAS_RTC
  Serial.println("[RTC] RTC non disponible sur ce modele");
  return;
#else
  if (!RTCManager::isAvailable()) {
    Serial.println("[RTC] RTC non disponible");
    return;
  }
  
  if (args.length() == 0) {
    Serial.println("[RTC] Usage: rtc-set <timestamp|DD/MM/YYYY HH:MM:SS>");
    Serial.println("[RTC] Exemples:");
    Serial.println("[RTC]   rtc-set 1704067200        (timestamp Unix)");
    Serial.println("[RTC]   rtc-set 18/01/2026 15:30:00");
    return;
  }
  
  // Vérifier si c'est un timestamp Unix (nombre uniquement)
  bool isTimestamp = true;
  for (unsigned int i = 0; i < args.length(); i++) {
    if (!isDigit(args.charAt(i))) {
      isTimestamp = false;
      break;
    }
  }
  
  if (isTimestamp) {
    // Timestamp Unix
    uint32_t timestamp = args.toInt();
    if (RTCManager::setUnixTime(timestamp)) {
      Serial.print("[RTC] Heure definie depuis timestamp: ");
      Serial.println(RTCManager::getDateTimeString());
    } else {
      Serial.println("[RTC] Erreur lors de la definition de l'heure");
    }
  } else {
    // Format DD/MM/YYYY HH:MM:SS
    DateTime dt = {0, 0, 0, 0, 0, 0, 0};
    
    // Parser la date et l'heure
    int day, month, year, hour, minute, second;
    if (sscanf(args.c_str(), "%d/%d/%d %d:%d:%d", &day, &month, &year, &hour, &minute, &second) == 6) {
      dt.day = day;
      dt.month = month;
      dt.year = year;
      dt.hour = hour;
      dt.minute = minute;
      dt.second = second;
      
      if (RTCManager::setDateTime(dt)) {
        Serial.print("[RTC] Heure definie: ");
        Serial.println(RTCManager::getDateTimeString());
      } else {
        Serial.println("[RTC] Erreur lors de la definition de l'heure");
        Serial.println("[RTC] Verifiez le format: DD/MM/YYYY HH:MM:SS");
      }
    } else {
      Serial.println("[RTC] Format invalide");
      Serial.println("[RTC] Utilisez: DD/MM/YYYY HH:MM:SS (ex: 18/01/2026 15:30:00)");
    }
  }
#endif
}

void SerialCommands::cmdRTCSync() {
#ifndef HAS_RTC
  Serial.println("[RTC] RTC non disponible sur ce modele");
  return;
#else
  if (!RTCManager::isAvailable()) {
    Serial.println("[RTC] RTC non disponible");
    return;
  }

  if (!WiFiManager::isConnected()) {
    Serial.println("[RTC] WiFi non connecte - connexion requise pour NTP");
    return;
  }

  // Synchroniser avec le fuseau horaire français (GMT+1/+2)
  if (RTCManager::syncWithNTP(0, 0)) {
    Serial.println("[RTC] Synchronisation NTP reussie");
  } else {
    Serial.println("[RTC] Echec synchronisation NTP");
  }
#endif
}

void SerialCommands::cmdTimezone() {
#ifndef HAS_RTC
  Serial.println("[RTC] RTC non disponible sur ce modele");
  return;
#else
  Serial.println("");
  Serial.println("========================================");
  Serial.println("  FUSEAU HORAIRE ACTUEL");
  Serial.println("========================================");

#ifdef HAS_SD
  const char* tz = RTCManager::getTimezoneId();
  if (tz && strlen(tz) > 0) {
    Serial.printf("Timezone: %s\n", tz);

    // Obtenir l'offset actuel avec DST
    if (RTCManager::isAvailable()) {
      DateTime now = RTCManager::getDateTime();
      #ifdef HAS_TIMEZONE_MANAGER
      #include "common/managers/timezone/timezone_manager.h"
      long offsetSeconds = TimezoneManager::getTotalOffsetSeconds(tz, now.year, now.month, now.day);
      int hours = offsetSeconds / 3600;
      int minutes = (abs(offsetSeconds) % 3600) / 60;
      if (offsetSeconds == 0) {
        Serial.println("Offset: GMT+0 (UTC)");
      } else if (offsetSeconds > 0) {
        Serial.printf("Offset: GMT+%d:%02d\n", hours, minutes);
      } else {
        Serial.printf("Offset: GMT%d:%02d\n", hours, minutes);
      }
      #else
      Serial.println("Offset: (TimezoneManager non disponible)");
      #endif
    }
  } else {
    Serial.println("Timezone: Non configuree (UTC par defaut)");
    Serial.println("Offset: GMT+0");
  }
#else
  Serial.println("Timezone: UTC (SD non disponible)");
  Serial.println("Offset: GMT+0");
#endif

  Serial.println("========================================");
  Serial.println("");
#endif
}

void SerialCommands::cmdApiPing() {
#ifndef HAS_WIFI
  Serial.println("[API-PING] WiFi non disponible sur ce modele");
  return;
#else
  if (!HAS_WIFI) {
    Serial.println("[API-PING] WiFi non disponible");
    return;
  }

  if (!WiFiManager::isConnected()) {
    Serial.println("[API-PING] WiFi non connecte");
    return;
  }

  Serial.println("[API-PING] Test de connectivite au serveur API...");
  Serial.print("[API-PING] URL cible: ");
  Serial.println(API_BASE_URL);

  // Construire l'URL de test (un endpoint simple)
  char url[256];
  snprintf(url, sizeof(url), "%s/api/health", API_BASE_URL);

  WiFiClientSecure client;
  client.setCACert(ISRG_ROOT_X1);  // Vérifier le certificat CA (protection MITM)
  HTTPClient http;
  http.begin(client, url);
  http.setTimeout(8000);

  unsigned long startTime = millis();
  int httpCode = http.GET();
  unsigned long elapsed = millis() - startTime;

  Serial.print("[API-PING] Reponse HTTP: ");
  Serial.print(httpCode);
  Serial.print(" (");
  Serial.print(elapsed);
  Serial.println(" ms)");

  if (httpCode > 0) {
    if (httpCode == 200) {
      Serial.println("[API-PING] ✓ Serveur accessible et repondant correctement");
    } else if (httpCode >= 400 && httpCode < 500) {
      Serial.print("[API-PING] ⚠ Client error (");
      Serial.print(httpCode);
      Serial.println(") - Verifiez l'URL ou l'authentification");
    } else if (httpCode >= 500) {
      Serial.print("[API-PING] ⚠ Serveur error (");
      Serial.print(httpCode);
      Serial.println(") - Le serveur a une erreur");
    } else {
      Serial.print("[API-PING] ✓ Reponse: ");
      Serial.println(httpCode);
    }
  } else if (httpCode == -1) {
    Serial.println("[API-PING] ✗ Erreur de connexion: Serveur inaccessible ou timeout");
    Serial.println("[API-PING] Verifiez:");
    Serial.println("  - L'adresse IP/hostname est correcte");
    Serial.println("  - Le port est accessible");
    Serial.println("  - Le firewall ne bloque pas la connexion");
    Serial.println("  - Le cable reseau/WiFi est connecte");
  } else {
    Serial.print("[API-PING] ✗ Erreur HTTP: ");
    Serial.println(httpCode);
  }

  http.end();
  client.stop();
  Serial.println("");
#endif
}

void SerialCommands::cmdPotentiometer() {
#ifndef HAS_POTENTIOMETER
  Serial.println("[POT] Potentiometre non disponible sur ce modele");
  return;
#else
  PotentiometerManager::printInfo();
#endif
}

void SerialCommands::cmdMemoryDebug() {
  Serial.println("");
  Serial.println("========== ANALYSE RAM DETAILLEE ==========");
  Serial.println("");
  
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t totalHeap = ESP.getHeapSize();
  uint32_t usedHeap = totalHeap - freeHeap;
  uint32_t usagePercent = (usedHeap * 100) / totalHeap;
  uint32_t freePercent = (freeHeap * 100) / totalHeap;
  
  Serial.print("RAM Totale: ");
  Serial.print(totalHeap / 1024);
  Serial.println(" KB");
  Serial.print("RAM Utilisee: ");
  Serial.print(usedHeap / 1024);
  Serial.print(" KB (");
  Serial.print(usagePercent);
  Serial.println("%)");
  Serial.print("RAM Libre: ");
  Serial.print(freeHeap / 1024);
  Serial.print(" KB (");
  Serial.print(freePercent);
  Serial.println("%)");
  Serial.println("");
  
  // Estimation par composant basée sur l'état actuel
  Serial.println("[Estimation par composant]");
  Serial.println("(Basee sur les valeurs typiques ESP32)");
  Serial.println("");
  
  uint32_t estimatedUsed = 0;
  uint32_t totalKB = totalHeap / 1024;
  
  // Helper pour afficher KB et %
  auto printComponent = [totalKB](const char* name, uint32_t minKB, uint32_t maxKB) {
    uint32_t avgKB = (minKB + maxKB) / 2;
    uint32_t percent = (avgKB * 100) / totalKB;
    Serial.print("  ");
    Serial.print(name);
    Serial.print("~");
    Serial.print(minKB);
    Serial.print("-");
    Serial.print(maxKB);
    Serial.print(" KB (");
    Serial.print(percent);
    Serial.println("%)");
    return avgKB;
  };
  
  // Firmware de base (~50-60 KB)
  estimatedUsed += printComponent("Firmware/Stack:      ", 50, 60);
  
  // WiFi
  if (WiFiManager::isConnected()) {
    estimatedUsed += printComponent("WiFi (connecte):     ", 40, 50);
  } else if (WiFiManager::isInitialized()) {
    estimatedUsed += printComponent("WiFi (init):         ", 25, 30);
  } else {
    Serial.println("  WiFi:                      Non init (0%)");
  }
  
  // BLE
  if (BLEManager::isInitialized()) {
    if (BLEManager::isConnected()) {
      estimatedUsed += printComponent("BLE (connecte):      ", 40, 50);
    } else {
      estimatedUsed += printComponent("BLE (advertising):   ", 30, 40);
    }
  } else {
    Serial.println("  BLE:                       Non init (0%)");
  }
  
  // MQTT
  #ifdef HAS_MQTT
  if (MqttManager::isConnected()) {
    estimatedUsed += printComponent("MQTT (connecte):   ", 20, 30);
  } else if (MqttManager::isInitialized()) {
    estimatedUsed += printComponent("MQTT (init):       ", 5, 10);
  } else {
    Serial.println("  MQTT:                    Non init (0%)");
  }
  #else
  Serial.println("  MQTT:                    Non disponible (0%)");
  #endif
  
  // LEDs (FastLED)
  if (LEDManager::isInitialized()) {
#ifdef NUM_LEDS
    uint32_t ledRam = (NUM_LEDS * 3 + 1023) / 1024;  // Arrondi supérieur
    if (ledRam < 1) ledRam = 1;
    uint32_t ledPercent = (ledRam * 100) / totalKB;
    Serial.print("  FastLED (");
    Serial.print(NUM_LEDS);
    Serial.print(" LEDs):    ~");
    Serial.print(ledRam);
    Serial.print(" KB (");
    Serial.print(ledPercent);
    Serial.println("%)");
    estimatedUsed += ledRam;
#else
    estimatedUsed += printComponent("FastLED:             ", 1, 1);
#endif
  }
  
  // SD Card
  if (SDManager::isAvailable()) {
    estimatedUsed += printComponent("SD Card:             ", 2, 5);
  }
  
  // NFC - compte même si WARNING car la lib est chargée
#ifdef HAS_NFC
  InitStatus nfcStatus = InitManager::getComponentStatus("nfc");
  if (nfcStatus == INIT_SUCCESS || nfcStatus == INIT_FAILED || nfcStatus == INIT_IN_PROGRESS) {
    // Wire (I2C) + PN532 lib sont chargées même si le module n'est pas détecté
    estimatedUsed += printComponent("NFC/I2C (PN532+Wire):", 8, 15);
  }
#endif
  
  // RTC (partage I2C avec NFC, donc pas de surcoût Wire)
#ifdef HAS_RTC
  if (RTCManager::isAvailable()) {
    Serial.print("  RTC (DS3231):              ~1-2 KB (");
    Serial.print((1 * 100) / totalKB);
    Serial.println("%)");
    estimatedUsed += 1;
  } else if (InitManager::getComponentStatus("rtc") != INIT_NOT_STARTED) {
    // RTC init tenté mais échoué - lib quand même chargée
    Serial.print("  RTC (init failed):         ~1 KB (");
    Serial.print((1 * 100) / totalKB);
    Serial.println("%)");
    estimatedUsed += 1;
  }
#endif
  
  // Potentiometre
#ifdef HAS_POTENTIOMETER
  if (PotentiometerManager::isAvailable()) {
    Serial.println("  Potentiometre:             <1 KB (0%)");
  }
#endif
  
  // FreeRTOS tasks
  estimatedUsed += printComponent("FreeRTOS tasks:      ", 10, 20);
  
  // Buffers divers (Serial, JSON, etc.)
  estimatedUsed += printComponent("Buffers (Serial,JSON):", 5, 10);
  
  Serial.println("");
  Serial.println("-------------------------------------------");
  uint32_t estimatedPercent = (estimatedUsed * 100) / totalKB;
  Serial.print("  Estimation totale:         ~");
  Serial.print(estimatedUsed);
  Serial.print(" KB (~");
  Serial.print(estimatedPercent);
  Serial.println("%)");
  
  Serial.print("  Utilisation reelle:        ");
  Serial.print(usedHeap / 1024);
  Serial.print(" KB (");
  Serial.print(usagePercent);
  Serial.println("%)");
  
  int32_t diff = (usedHeap / 1024) - estimatedUsed;
  int32_t diffPercent = usagePercent - estimatedPercent;
  Serial.print("  Difference:                ");
  if (diff > 0) Serial.print("+");
  Serial.print(diff);
  Serial.print(" KB (");
  if (diffPercent > 0) Serial.print("+");
  Serial.print(diffPercent);
  Serial.println("%)");
  
  if (diff > 20) {
    Serial.println("");
    Serial.println("[!] Difference importante detectee!");
    Serial.println("    Causes possibles:");
    Serial.println("    - Fuites memoire");
    Serial.println("    - Gros buffers JSON non liberes");
    Serial.println("    - Strings dynamiques accumulees");
  }
  
  Serial.println("");
  Serial.println("============================================");
  
  // Conseil
  if (freeHeap < 30000) {
    Serial.println("");
    Serial.println("[CONSEIL] RAM critique! Options:");
    Serial.println("  1. Desactiver BLE si WiFi suffit");
    Serial.println("  2. Reduire NUM_LEDS si possible");
    Serial.println("  3. Utiliser un ESP32 avec PSRAM");
  }
}

void SerialCommands::cmdNFCRead(const String& args) {
#ifndef HAS_NFC
  Serial.println("[NFC] NFC non disponible sur ce modele");
  return;
#else
  if (!NFCManager::isAvailable()) {
    Serial.println("[NFC] NFC non disponible");
    return;
  }
  
  // Lire l'UID du tag
  uint8_t uid[10];
  uint8_t uidLength;
  
  Serial.println("[NFC] Approchez un tag NFC...");
  
  if (!NFCManager::readTagUID(uid, &uidLength, 10000)) {
    Serial.println("[NFC] ERREUR: Aucun tag detecte apres 10 secondes");
    return;
  }
  
  // Afficher l'UID
  Serial.print("[NFC] Tag detecte - UID: ");
  for (uint8_t i = 0; i < uidLength; i++) {
    if (uid[i] < 0x10) Serial.print("0");
    Serial.print(uid[i], HEX);
    if (i < uidLength - 1) Serial.print(":");
  }
  Serial.println();
  Serial.print("[NFC] Longueur UID: ");
  Serial.print(uidLength);
  Serial.println(" bytes");
  
  // Si un numéro de bloc est spécifié, lire ce bloc
  if (args.length() > 0) {
    int blockNumber = args.toInt();
    
    if (blockNumber < 0 || blockNumber > 63) {
      Serial.println("[NFC] ERREUR: Numero de bloc invalide (0-63)");
      return;
    }
    
    uint8_t data[16];
    Serial.print("[NFC] Lecture du bloc ");
    Serial.print(blockNumber);
    Serial.println("...");
    
    if (!NFCManager::readBlock(blockNumber, data, uid, uidLength)) {
      Serial.println("[NFC] ERREUR: Echec de lecture du bloc");
      return;
    }
    
    // Afficher les données du bloc
    Serial.print("[NFC] Bloc ");
    Serial.print(blockNumber);
    Serial.println(" (hex):");
    for (uint8_t i = 0; i < 16; i++) {
      if (data[i] < 0x10) Serial.print("0");
      Serial.print(data[i], HEX);
      Serial.print(" ");
      if ((i + 1) % 8 == 0) Serial.println();
    }
    Serial.println();
    
    // Afficher en ASCII (si possible)
    Serial.print("[NFC] Bloc ");
    Serial.print(blockNumber);
    Serial.println(" (ASCII):");
    for (uint8_t i = 0; i < 16; i++) {
      if (data[i] >= 32 && data[i] < 127) {
        Serial.print((char)data[i]);
      } else {
        Serial.print(".");
      }
    }
    Serial.println();
  }
#endif
}

void SerialCommands::cmdNFCWrite(const String& args) {
#ifndef HAS_NFC
  Serial.println("[NFC] NFC non disponible sur ce modele");
  return;
#else
  if (!NFCManager::isAvailable()) {
    Serial.println("[NFC] NFC non disponible");
    return;
  }
  
  if (args.length() == 0) {
    Serial.println("[NFC] Usage: nfc-write <block> <data>");
    Serial.println("[NFC] Exemple: nfc-write 4 Hello World!");
    Serial.println("[NFC] Note: Le bloc doit etre entre 0 et 63");
    Serial.println("[NFC]       Les donnees seront tronquees a 16 bytes");
    return;
  }
  
  // Séparer le numéro de bloc et les données
  int spaceIndex = args.indexOf(' ');
  if (spaceIndex <= 0) {
    Serial.println("[NFC] ERREUR: Format invalide. Utilisez: nfc-write <block> <data>");
    return;
  }
  
  String blockStr = args.substring(0, spaceIndex);
  String dataStr = args.substring(spaceIndex + 1);
  
  int blockNumber = blockStr.toInt();
  
  if (blockNumber < 0 || blockNumber > 63) {
    Serial.println("[NFC] ERREUR: Numero de bloc invalide (0-63)");
    return;
  }
  
  // Préparer les données (16 bytes max)
  uint8_t data[16];
  memset(data, 0, sizeof(data));
  
  uint8_t dataLength = dataStr.length();
  if (dataLength > 16) {
    dataLength = 16;
    Serial.println("[NFC] ATTENTION: Donnees tronquees a 16 bytes");
  }
  
  memcpy(data, dataStr.c_str(), dataLength);
  
  // Lire l'UID du tag d'abord
  uint8_t uid[10];
  uint8_t uidLength;
  
  Serial.println("[NFC] Approchez un tag NFC...");
  
  if (!NFCManager::readTagUID(uid, &uidLength, 10000)) {
    Serial.println("[NFC] ERREUR: Aucun tag detecte apres 10 secondes");
    return;
  }
  
  Serial.print("[NFC] Tag detecte - UID: ");
  for (uint8_t i = 0; i < uidLength; i++) {
    if (uid[i] < 0x10) Serial.print("0");
    Serial.print(uid[i], HEX);
    if (i < uidLength - 1) Serial.print(":");
  }
  Serial.println();
  
  // Écrire le bloc
  Serial.print("[NFC] Ecriture du bloc ");
  Serial.print(blockNumber);
  Serial.println("...");
  
  if (!NFCManager::writeBlock(blockNumber, data, uid, uidLength)) {
    Serial.println("[NFC] ERREUR: Echec d'ecriture du bloc");
    Serial.println("[NFC] Verifiez que le tag n'est pas en lecture seule");
    return;
  }
  
  Serial.print("[NFC] Bloc ");
  Serial.print(blockNumber);
  Serial.println(" ecrit avec succes!");
  
  // Afficher les données écrites
  Serial.print("[NFC] Donnees ecrites (hex): ");
  for (uint8_t i = 0; i < 16; i++) {
    if (data[i] < 0x10) Serial.print("0");
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
#endif
}

void SerialCommands::cmdConfigList() {
  if (!SDManager::isAvailable()) {
    Serial.println("[CONFIG] Carte SD non disponible");
    return;
  }
  
  if (!SDManager::configFileExists()) {
    Serial.println("[CONFIG] Fichier config.json non trouve");
    return;
  }
  
  // Ouvrir et lire le fichier config.json
  File configFile = SD.open("/config.json", FILE_READ);
  if (!configFile) {
    Serial.println("[CONFIG] Erreur ouverture config.json");
    return;
  }
  
  // Lire le contenu
  size_t fileSize = configFile.size();
  if (fileSize == 0) {
    configFile.close();
    Serial.println("[CONFIG] Fichier config.json vide");
    return;
  }
  
  char* jsonBuffer = new char[fileSize + 1];
  if (!jsonBuffer) {
    configFile.close();
    Serial.println("[CONFIG] Erreur allocation memoire");
    return;
  }
  
  size_t bytesRead = configFile.readBytes(jsonBuffer, fileSize);
  jsonBuffer[bytesRead] = '\0';
  configFile.close();
  
  // Parser le JSON
  // Note: StaticJsonDocument est déprécié mais toujours fonctionnel dans ArduinoJson v7
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  StaticJsonDocument<2048> doc;
  #pragma GCC diagnostic pop
  DeserializationError error = deserializeJson(doc, jsonBuffer);
  delete[] jsonBuffer;
  
  if (error) {
    Serial.print("[CONFIG] Erreur parsing JSON: ");
    Serial.println(error.c_str());
    return;
  }
  
  // Afficher toutes les clés
  Serial.println("");
  Serial.println("========== config.json ==========");
  
  JsonObject root = doc.as<JsonObject>();
  for (JsonPair kv : root) {
    Serial.print("  ");
    Serial.print(kv.key().c_str());
    Serial.print(" = ");
    
    if (kv.value().is<const char*>()) {
      // Masquer les mots de passe
      String key = kv.key().c_str();
      if (key.indexOf("password") >= 0 || key.indexOf("secret") >= 0) {
        Serial.println("********");
      } else {
        Serial.println(kv.value().as<const char*>());
      }
    } else if (kv.value().is<int>()) {
      Serial.println(kv.value().as<int>());
    } else if (kv.value().is<float>()) {
      Serial.println(kv.value().as<float>());
    } else if (kv.value().is<bool>()) {
      Serial.println(kv.value().as<bool>() ? "true" : "false");
    } else {
      Serial.println("(objet/tableau)");
    }
  }
  
  Serial.println("=================================");
}

void SerialCommands::cmdConfigGet(const String& args) {
  if (!SDManager::isAvailable()) {
    Serial.println("[CONFIG] Carte SD non disponible");
    return;
  }
  
  if (args.length() == 0) {
    Serial.println("[CONFIG] Usage: config-get <key>");
    Serial.println("[CONFIG] Exemple: config-get wifi_ssid");
    return;
  }
  
  if (!SDManager::configFileExists()) {
    Serial.println("[CONFIG] Fichier config.json non trouve");
    return;
  }
  
  // Ouvrir et lire le fichier
  File configFile = SD.open("/config.json", FILE_READ);
  if (!configFile) {
    Serial.println("[CONFIG] Erreur ouverture config.json");
    return;
  }
  
  size_t fileSize = configFile.size();
  char* jsonBuffer = new char[fileSize + 1];
  if (!jsonBuffer) {
    configFile.close();
    Serial.println("[CONFIG] Erreur allocation memoire");
    return;
  }
  
  size_t bytesRead = configFile.readBytes(jsonBuffer, fileSize);
  jsonBuffer[bytesRead] = '\0';
  configFile.close();
  
  // Parser le JSON
  // Note: StaticJsonDocument est déprécié mais toujours fonctionnel dans ArduinoJson v7
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  StaticJsonDocument<2048> doc;
  #pragma GCC diagnostic pop
  DeserializationError error = deserializeJson(doc, jsonBuffer);
  delete[] jsonBuffer;
  
  if (error) {
    Serial.print("[CONFIG] Erreur parsing JSON: ");
    Serial.println(error.c_str());
    return;
  }
  
  // Chercher la clé
  String key = args;
  key.trim();
  
  if (doc[key].isNull()) {
    Serial.print("[CONFIG] Cle '");
    Serial.print(key);
    Serial.println("' non trouvee");
    return;
  }
  
  Serial.print("[CONFIG] ");
  Serial.print(key);
  Serial.print(" = ");
  
  JsonVariant value = doc[key];
  if (value.is<const char*>()) {
    // Masquer les mots de passe
    if (key.indexOf("password") >= 0 || key.indexOf("secret") >= 0) {
      Serial.println("********");
    } else {
      Serial.println(value.as<const char*>());
    }
  } else if (value.is<int>()) {
    Serial.println(value.as<int>());
  } else if (value.is<float>()) {
    Serial.println(value.as<float>());
  } else if (value.is<bool>()) {
    Serial.println(value.as<bool>() ? "true" : "false");
  } else {
    Serial.println("(objet/tableau)");
  }
}

void SerialCommands::cmdConfigSet(const String& args) {
  if (!SDManager::isAvailable()) {
    Serial.println("[CONFIG] Carte SD non disponible");
    return;
  }
  
  if (args.length() == 0) {
    Serial.println("[CONFIG] Usage: config-set <key> <value>");
    Serial.println("[CONFIG] Exemples:");
    Serial.println("[CONFIG]   config-set my_custom_key my_value");
    Serial.println("[CONFIG]   config-set led_brightness 128");
    return;
  }
  
  // Séparer la clé et la valeur
  int spaceIndex = args.indexOf(' ');
  if (spaceIndex <= 0) {
    Serial.println("[CONFIG] Erreur: format invalide. Utilisez: config-set <key> <value>");
    return;
  }
  
  String key = args.substring(0, spaceIndex);
  String value = args.substring(spaceIndex + 1);
  key.trim();
  value.trim();
  
  if (key.length() == 0 || value.length() == 0) {
    Serial.println("[CONFIG] Erreur: cle ou valeur vide");
    return;
  }
  
  // Lire le fichier existant ou créer un nouveau document
  // Note: StaticJsonDocument est déprécié mais toujours fonctionnel dans ArduinoJson v7
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  StaticJsonDocument<2048> doc;
  #pragma GCC diagnostic pop
  
  if (SDManager::configFileExists()) {
    File configFile = SD.open("/config.json", FILE_READ);
    if (configFile) {
      size_t fileSize = configFile.size();
      if (fileSize > 0) {
        char* jsonBuffer = new char[fileSize + 1];
        if (jsonBuffer) {
          size_t bytesRead = configFile.readBytes(jsonBuffer, fileSize);
          jsonBuffer[bytesRead] = '\0';
          deserializeJson(doc, jsonBuffer);
          delete[] jsonBuffer;
        }
      }
      configFile.close();
    }
  }
  
  // Déterminer le type de valeur et l'ajouter
  // Essayer de parser comme nombre entier
  bool isInt = true;
  bool isFloat = false;
  bool hasDecimal = false;
  
  for (unsigned int i = 0; i < value.length(); i++) {
    char c = value.charAt(i);
    if (c == '-' && i == 0) continue;
    if (c == '.') {
      if (hasDecimal) {
        isInt = false;
        isFloat = false;
        break;
      }
      hasDecimal = true;
      isFloat = true;
      isInt = false;
      continue;
    }
    if (!isDigit(c)) {
      isInt = false;
      isFloat = false;
      break;
    }
  }
  
  // Vérifier si c'est un booléen
  String valueLower = value;
  valueLower.toLowerCase();
  bool isBool = (valueLower == "true" || valueLower == "false");
  
  // Ajouter la valeur avec le bon type
  if (isBool) {
    doc[key] = (valueLower == "true");
    Serial.print("[CONFIG] ");
    Serial.print(key);
    Serial.print(" = ");
    Serial.print(valueLower == "true" ? "true" : "false");
    Serial.println(" (bool)");
  } else if (isInt) {
    doc[key] = value.toInt();
    Serial.print("[CONFIG] ");
    Serial.print(key);
    Serial.print(" = ");
    Serial.print(value.toInt());
    Serial.println(" (int)");
  } else if (isFloat) {
    doc[key] = value.toFloat();
    Serial.print("[CONFIG] ");
    Serial.print(key);
    Serial.print(" = ");
    Serial.print(value.toFloat());
    Serial.println(" (float)");
  } else {
    doc[key] = value;
    Serial.print("[CONFIG] ");
    Serial.print(key);
    Serial.print(" = ");
    Serial.print(value);
    Serial.println(" (string)");
  }
  
  // Sauvegarder le fichier
  File configFile = SD.open("/config.json", FILE_WRITE);
  if (!configFile) {
    Serial.println("[CONFIG] Erreur: impossible d'ouvrir config.json en ecriture");
    return;
  }
  
  size_t bytesWritten = serializeJson(doc, configFile);
  configFile.close();
  
  if (bytesWritten > 0) {
    Serial.println("[CONFIG] Sauvegarde OK");
  } else {
    Serial.println("[CONFIG] Erreur lors de la sauvegarde");
  }
}

// ============================================
// Commandes Audio
// ============================================

void SerialCommands::cmdAudio() {
#ifdef HAS_AUDIO
  AudioManager::printStatus();
#else
  Serial.println("[AUDIO] Audio non disponible sur ce modele");
#endif
}

void SerialCommands::cmdAudioPlay(const String& args) {
#ifdef HAS_AUDIO
  if (args.length() == 0) {
    Serial.println("[AUDIO] Usage: play <fichier>");
    Serial.println("[AUDIO] Exemple: play /music/song.mp3");
    return;
  }
  
  String path = args;
  // Ajouter le / au début si absent
  if (!path.startsWith("/")) {
    path = "/" + path;
  }
  
  if (AudioManager::play(path.c_str())) {
    Serial.printf("[AUDIO] Lecture de: %s\n", path.c_str());
  }
#else
  Serial.println("[AUDIO] Audio non disponible sur ce modele");
#endif
}

void SerialCommands::cmdAudioStop() {
#ifdef HAS_AUDIO
  AudioManager::stop();
#else
  Serial.println("[AUDIO] Audio non disponible sur ce modele");
#endif
}

void SerialCommands::cmdAudioPause() {
#ifdef HAS_AUDIO
  if (AudioManager::isPlaying()) {
    AudioManager::pause();
  } else if (AudioManager::isPaused()) {
    Serial.println("[AUDIO] Deja en pause");
  } else {
    Serial.println("[AUDIO] Aucune lecture en cours");
  }
#else
  Serial.println("[AUDIO] Audio non disponible sur ce modele");
#endif
}

void SerialCommands::cmdAudioResume() {
#ifdef HAS_AUDIO
  if (AudioManager::isPaused()) {
    AudioManager::resume();
  } else if (AudioManager::isPlaying()) {
    Serial.println("[AUDIO] Lecture deja en cours");
  } else {
    Serial.println("[AUDIO] Aucune lecture en pause");
  }
#else
  Serial.println("[AUDIO] Audio non disponible sur ce modele");
#endif
}

void SerialCommands::cmdAudioVolume(const String& args) {
#ifdef HAS_AUDIO
  if (args.length() == 0) {
    // Afficher le volume actuel
    Serial.printf("[AUDIO] Volume actuel: %d%%\n", AudioManager::getVolume());
    return;
  }
  
  // Parser le volume
  int volume = args.toInt();
  
  // Vérifier les limites (0-100%)
  if (volume < 0 || volume > 100) {
    Serial.println("[AUDIO] Erreur: le volume doit etre entre 0 et 100 (%)");
    return;
  }
  
  AudioManager::setVolume(volume);
#else
  Serial.println("[AUDIO] Audio non disponible sur ce modele");
#endif
}

void SerialCommands::cmdAudioList(const String& args) {
#ifdef HAS_AUDIO
  if (!SDManager::isAvailable()) {
    Serial.println("[AUDIO] Erreur: carte SD non disponible");
    return;
  }
  
  String path = args;
  if (path.length() == 0) {
    path = "/";
  }
  
  // Ajouter le / au début si absent
  if (!path.startsWith("/")) {
    path = "/" + path;
  }
  
  File dir = SD.open(path);
  if (!dir) {
    Serial.printf("[AUDIO] Erreur: impossible d'ouvrir %s\n", path.c_str());
    return;
  }
  
  if (!dir.isDirectory()) {
    Serial.printf("[AUDIO] %s n'est pas un dossier\n", path.c_str());
    dir.close();
    return;
  }
  
  Serial.printf("\n[AUDIO] Contenu de %s:\n", path.c_str());
  Serial.println("----------------------------------------");
  
  int fileCount = 0;
  int audioCount = 0;
  
  File file = dir.openNextFile();
  while (file) {
    String name = file.name();
    
    if (file.isDirectory()) {
      Serial.printf("  [DIR]  %s/\n", name.c_str());
    } else {
      // Vérifier si c'est un fichier audio
      String nameLower = name;
      nameLower.toLowerCase();
      bool isAudio = nameLower.endsWith(".mp3") || 
                     nameLower.endsWith(".wav") ||
                     nameLower.endsWith(".flac") ||
                     nameLower.endsWith(".aac") ||
                     nameLower.endsWith(".ogg");
      
      if (isAudio) {
        Serial.printf("  [MP3]  %s (%d bytes)\n", name.c_str(), file.size());
        audioCount++;
      } else {
        Serial.printf("  [---]  %s (%d bytes)\n", name.c_str(), file.size());
      }
      fileCount++;
    }
    
    file = dir.openNextFile();
  }
  
  dir.close();
  
  Serial.println("----------------------------------------");
  Serial.printf("[AUDIO] %d fichiers (%d audio)\n", fileCount, audioCount);
#else
  Serial.println("[AUDIO] Audio non disponible sur ce modele");
#endif
}

void SerialCommands::cmdLEDTest() {
#ifdef HAS_LED
  if (!LEDManager::isInitialized()) {
    Serial.println("[LED-TEST] LED Manager non initialise");
    return;
  }
  
  // Réveiller les LEDs pour désactiver le sleep mode lors des tests
  LEDManager::wakeUp();
  
  LEDManager::testLEDsSequential();
#else
  Serial.println("[LED-TEST] LEDs non disponibles sur ce modele");
#endif
}

void SerialCommands::cmdLCDTest() {
#ifdef HAS_LCD
  LCDManager::testLCD();
#else
  Serial.println("[LCD-TEST] LCD non disponible sur ce modele");
#endif
}

void SerialCommands::cmdLCDReset() {
#ifdef HAS_LCD
  if (!LCDManager::isAvailable()) {
    Serial.println("[LCD-RESET] LCD non disponible");
    return;
  }
  LCDManager::waitDMA();
  LCDManager::reinitDisplay();
  Serial.println("[LCD-RESET] Ecran reinitialise");
#else
  Serial.println("[LCD-RESET] LCD non disponible sur ce modele");
#endif
}

void SerialCommands::cmdLCDFps() {
#ifdef HAS_LCD
  LCDManager::testFPS();
#else
  Serial.println("[LCD-FPS] LCD non disponible sur ce modele");
#endif
}

void SerialCommands::cmdLCDPlayMjpeg(const String& args) {
#ifdef HAS_LCD
  const char* path = args.length() > 0 ? args.c_str() : "/video.mjpeg";
  LCDManager::playMjpegFromSD(path);
#else
  (void)args;
  Serial.println("[LCD-PLAY] LCD non disponible sur ce modele");
#endif
}

// ============================================
// Commandes Touch (TTP223)
// ============================================

void SerialCommands::cmdTouch(const String& args) {
#ifdef HAS_TOUCH
  if (!TouchManager::isInitialized()) {
    Serial.println("[TOUCH] Capteur non initialise");
    return;
  }

  String a = args;
  a.trim();
  a.toLowerCase();

  if (a.length() == 0 || a == "status" || a == "info") {
    TouchManager::printStatus();
    return;
  }
  if (a == "read" || a == "raw") {
    TouchManager::update();  // Mettre à jour le debounce avant lecture
    bool touched = TouchManager::isTouched();
    bool raw = TouchManager::readRaw();
    Serial.printf("[TOUCH] Debounce: %s | Brut: %s\n", touched ? "TOUCHE" : "RELACHE", raw ? "HIGH" : "LOW");
    return;
  }

  Serial.println("[TOUCH] Usage: touch [status|read]");
#else
  (void)args;
  Serial.println("[TOUCH] Capteur tactile non disponible sur ce modele");
#endif
}

// ============================================
// Commandes Capteur environnement (AHT20 + BMP280)
// ============================================

void SerialCommands::cmdEnv(const String& args) {
#ifdef HAS_ENV_SENSOR
  if (!EnvSensorManager::isInitialized()) {
    Serial.println("[ENV] Capteur non initialise");
    return;
  }
  (void)args;
  EnvSensorManager::printInfo();
#else
  (void)args;
  Serial.println("[ENV] Capteur environnement (AHT20+BMP280) non disponible sur ce modele");
#endif
}

// ============================================
// Commandes Vibreur
// ============================================

void SerialCommands::cmdVibrator(const String& args) {
#ifdef HAS_VIBRATOR
  if (!VibratorManager::isInitialized()) {
    Serial.println("[VIBRATOR] Vibreur non initialise");
    return;
  }

  String a = args;
  a.trim();
  a.toLowerCase();

  if (a.length() == 0 || a == "status" || a == "info") {
    VibratorManager::printStatus();
    return;
  }

  if (a == "on") {
    VibratorManager::setOn(true);
    Serial.println("[VIBRATOR] ON");
    return;
  }
  if (a == "off") {
    VibratorManager::stop();
    Serial.println("[VIBRATOR] OFF");
    return;
  }
  if (a == "toggle") {
    VibratorManager::setOn(!VibratorManager::isOn());
    Serial.printf("[VIBRATOR] %s\n", VibratorManager::isOn() ? "ON" : "OFF");
    return;
  }
  if (a == "test") {
    Serial.println("[VIBRATOR] Test 500 ms...");
    VibratorManager::setOn(true);
    delay(500);
    VibratorManager::stop();
    Serial.println("[VIBRATOR] Test termine");
    return;
  }
  if (a.startsWith("intensity ")) {
    String valStr = a.substring(10);
    valStr.trim();
    if (valStr.length() == 0) {
      Serial.printf("[VIBRATOR] Intensite actuelle: %d/255\n", VibratorManager::getIntensity());
      return;
    }
    int val = valStr.toInt();
    if (val < 0 || val > 255) {
      Serial.println("[VIBRATOR] Erreur: intensite doit etre entre 0 et 255");
      return;
    }
    VibratorManager::setIntensity((uint8_t)val);
    Serial.printf("[VIBRATOR] Intensite definie a %d/255\n", val);
    return;
  }

  // Effets : court, long, saccadé, pulsation, double tap
  String effectName = a;
  if (effectName.startsWith("effect ")) {
    effectName = effectName.substring(7);
    effectName.trim();
  }
  if (effectName == "short" || effectName == "court") {
    Serial.println("[VIBRATOR] Effet court...");
    VibratorManager::playEffect(VibratorManager::EFFECT_SHORT);
    Serial.println("[VIBRATOR] OK");
    return;
  }
  if (effectName == "long") {
    Serial.println("[VIBRATOR] Effet long...");
    VibratorManager::playEffect(VibratorManager::EFFECT_LONG);
    Serial.println("[VIBRATOR] OK");
    return;
  }
  if (effectName == "saccade" || effectName == "saccadé" || effectName == "jerky") {
    Serial.println("[VIBRATOR] Effet saccade...");
    VibratorManager::playEffect(VibratorManager::EFFECT_JERKY);
    Serial.println("[VIBRATOR] OK");
    return;
  }
  if (effectName == "pulse" || effectName == "pulsation") {
    Serial.println("[VIBRATOR] Effet pulsation...");
    VibratorManager::playEffect(VibratorManager::EFFECT_PULSE);
    Serial.println("[VIBRATOR] OK");
    return;
  }
  if (effectName == "doubletap" || effectName == "double-tap" || effectName == "double_tap") {
    Serial.println("[VIBRATOR] Effet double tap...");
    VibratorManager::playEffect(VibratorManager::EFFECT_DOUBLE_TAP);
    Serial.println("[VIBRATOR] OK");
    return;
  }

  Serial.println("[VIBRATOR] Usage: vibrator [on|off|toggle|test|status|intensity [0-255]|short|long|saccade|pulse|doubletap]");
#else
  (void)args;
  Serial.println("[VIBRATOR] Vibreur non disponible sur ce modele");
#endif
}

void SerialCommands::cmdOta(const String& args) {
#ifndef HAS_WIFI
  Serial.println("[OTA] OTA necessite le WiFi (non disponible sur ce modele)");
  return;
#else
  if (args.length() == 0) {
    Serial.println("[OTA] Usage: ota <version>");
    Serial.println("[OTA] Exemple: ota 1.0.26");
    return;
  }

  if (!WiFiManager::isConnected()) {
    Serial.println("[OTA] WiFi non connecte. Connectez-vous (wifi-connect) puis reessayez.");
    return;
  }

  String version = args;
  version.trim();

  Serial.print("[OTA] Lancement mise a jour vers ");
  Serial.print(version);
  Serial.println("... (tache dediee)");

  if (!OTAManager::startUpdateTask(version.c_str())) {
    Serial.println("[OTA] Erreur: version invalide ou impossible de creer la tache OTA.");
    return;
  }
#endif
}
