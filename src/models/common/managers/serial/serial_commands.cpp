#include "serial_commands.h"
#include "serial_manager.h"
#include "../led/led_manager.h"
#include "../init/init_manager.h"
#include "../sd/sd_manager.h"
#include <SD.h>
#include <ArduinoJson.h>
#include "../ble/ble_manager.h"
#include "../wifi/wifi_manager.h"
#ifdef HAS_PUBNUB
#include "../pubnub/pubnub_manager.h"
#endif
#include "../rtc/rtc_manager.h"
#include "../potentiometer/potentiometer_manager.h"
#include "../nfc/nfc_manager.h"
#ifdef HAS_AUDIO
#include "../audio/audio_manager.h"
#endif
#include "../../../model_serial_commands.h"
#ifdef HAS_PUBNUB
#include "../../../model_pubnub_routes.h"
#endif
#include "../../../model_config.h"
#include <Arduino.h>

// Variables statiques
bool SerialCommands::initialized = false;
String SerialCommands::inputBuffer = "";

void SerialCommands::init() {
  if (initialized) {
    return;
  }
  
  initialized = true;
  inputBuffer = "";
  
  // Initialiser seulement si Serial est disponible (USB connecté)
  if (Serial) {
    Serial.println("[SERIAL] Systeme de commandes initialise");
    Serial.println("[SERIAL] Tapez 'help' pour voir les commandes disponibles");
  }
}

void SerialCommands::update() {
  // Vérifier que Serial est disponible avant d'essayer de lire
  if (!Serial || !Serial.available()) {
    return;
  }
  
  // Lire les caractères disponibles
  while (Serial.available()) {
    char c = Serial.read();
    
    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() > 0) {
        processCommand(inputBuffer);
        inputBuffer = "";
      }
    } else if (c == '\b' || c == 127) {
      // Backspace
      if (inputBuffer.length() > 0) {
        inputBuffer.remove(inputBuffer.length() - 1);
        Serial.print("\b \b");
      }
    } else if (c >= 32 && c <= 126) {
      // Caractère imprimable
      inputBuffer += c;
      Serial.print(c);
    }
  }
}

void SerialCommands::processCommand(const String& command) {
  if (command.length() == 0) {
    return;
  }
  
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
  } else if (cmd == "wifi" || cmd == "wifi-status") {
    cmdWiFi();
  } else if (cmd == "wifi-set") {
    cmdWiFiSet(args);
  } else if (cmd == "wifi-connect") {
    cmdWiFiConnect();
  } else if (cmd == "wifi-disconnect") {
    cmdWiFiDisconnect();
  #ifdef HAS_PUBNUB
  } else if (cmd == "pubnub" || cmd == "pubnub-status") {
    cmdPubNub();
  } else if (cmd == "pubnub-connect") {
    cmdPubNubConnect();
  } else if (cmd == "pubnub-disconnect") {
    cmdPubNubDisconnect();
  } else if (cmd == "pubnub-publish" || cmd == "pubnub-pub") {
    cmdPubNubPublish(args);
  } else if (cmd == "pubnub-routes" || cmd == "routes") {
    cmdPubNubRoutes();
  #endif
  } else if (cmd == "rtc" || cmd == "time" || cmd == "date") {
    cmdRTC();
  } else if (cmd == "rtc-set" || cmd == "time-set") {
    cmdRTCSet(args);
  } else if (cmd == "rtc-sync" || cmd == "ntp" || cmd == "ntp-sync") {
    cmdRTCSync();
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
  #ifdef HAS_LED
  } else if (cmd == "led-test" || cmd == "test-led" || cmd == "testleds") {
    cmdLEDTest();
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
  
  #ifdef HAS_BLE
  if (HAS_BLE) {
    Serial.println("  ble, bluetooth   - Afficher l'etat de connexion BLE");
  }
  #endif
  
  #ifdef HAS_WIFI
  if (HAS_WIFI) {
    Serial.println("  wifi             - Afficher l'etat de connexion WiFi");
    Serial.println("  wifi-set <ssid> [password] - Configurer le WiFi");
    Serial.println("  wifi-connect     - Se connecter au WiFi configure");
    Serial.println("  wifi-disconnect  - Se deconnecter du WiFi");
  }
  #endif
  
  #ifdef HAS_PUBNUB
  if (HAS_PUBNUB) {
    Serial.println("  pubnub           - Afficher l'etat PubNub");
    Serial.println("  pubnub-connect   - Se connecter a PubNub");
    Serial.println("  pubnub-disconnect - Se deconnecter de PubNub");
    Serial.println("  pubnub-pub <msg> - Publier un message");
    Serial.println("  pubnub-routes    - Afficher les routes PubNub disponibles");
  }
  #endif
  
  #ifdef HAS_RTC
  if (HAS_RTC) {
    Serial.println("  rtc, time, date  - Afficher l'heure et la date du RTC");
    Serial.println("  rtc-set <timestamp|DD/MM/YYYY HH:MM:SS> - Definir l'heure");
    Serial.println("  rtc-sync, ntp    - Synchroniser l'heure via NTP (WiFi requis)");
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
  
  Serial.println("  config-list, config - Afficher toutes les cles de config.json");
  Serial.println("  config-get <key>   - Lire une cle de config.json");
  Serial.println("  config-set <key> <value> - Definir une cle dans config.json");
  
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
    uint8_t brightness = (uint8_t)((percent * 255 + 50) / 100);
    
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

void SerialCommands::cmdPubNub() {
  PubNubManager::printInfo();
}

void SerialCommands::cmdPubNubConnect() {
#ifndef HAS_PUBNUB
  Serial.println("[PUBNUB] PubNub non disponible sur ce modele");
  return;
#else
  if (!PubNubManager::isInitialized()) {
    // Tenter d'initialiser
    if (!PubNubManager::init()) {
      Serial.println("[PUBNUB] Echec initialisation");
      return;
    }
  }
  
  // Vérifier si déjà connecté
  if (PubNubManager::isConnected()) {
    Serial.println("[PUBNUB] Deja connecte");
    return;
  }
  
  // Se connecter
  Serial.println("[PUBNUB] Tentative de connexion...");
  if (PubNubManager::connect()) {
    Serial.println("[PUBNUB] Connexion reussie!");
  } else {
    Serial.println("[PUBNUB] Echec de connexion");
  }
#endif
}

void SerialCommands::cmdPubNubDisconnect() {
#ifndef HAS_PUBNUB
  Serial.println("[PUBNUB] PubNub non disponible sur ce modele");
  return;
#else
  if (!PubNubManager::isConnected()) {
    Serial.println("[PUBNUB] Pas connecte");
    return;
  }
  
  PubNubManager::disconnect();
  Serial.println("[PUBNUB] Deconnecte");
#endif
}

void SerialCommands::cmdPubNubPublish(const String& args) {
#ifndef HAS_PUBNUB
  Serial.println("[PUBNUB] PubNub non disponible sur ce modele");
  return;
#else
  if (!PubNubManager::isConnected()) {
    Serial.println("[PUBNUB] Non connecte");
    return;
  }
  
  if (args.length() == 0) {
    // Publier le statut par défaut
    if (PubNubManager::publishStatus()) {
      Serial.println("[PUBNUB] Statut publie");
    } else {
      Serial.println("[PUBNUB] Echec publication");
    }
  } else {
    // Publier le message
    if (PubNubManager::publish(args.c_str())) {
      Serial.print("[PUBNUB] Message publie: ");
      Serial.println(args);
    } else {
      Serial.println("[PUBNUB] Echec publication");
    }
  }
#endif
}

void SerialCommands::cmdPubNubRoutes() {
#ifndef HAS_PUBNUB
  Serial.println("[PUBNUB] PubNub non disponible sur ce modele");
  return;
#else
  ModelPubNubRoutes::printRoutes();
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
  if (RTCManager::syncWithNTPFrance()) {
    Serial.println("[RTC] Synchronisation NTP reussie");
  } else {
    Serial.println("[RTC] Echec synchronisation NTP");
  }
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
  
  // PubNub
  #ifdef HAS_PUBNUB
  if (PubNubManager::isConnected()) {
    estimatedUsed += printComponent("PubNub (connecte):   ", 20, 30);
  } else if (PubNubManager::isInitialized()) {
    estimatedUsed += printComponent("PubNub (init):       ", 5, 10);
  } else {
    Serial.println("  PubNub:                    Non init (0%)");
  }
  #else
  Serial.println("  PubNub:                    Non disponible (0%)");
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
    #ifdef HAS_PUBNUB
    Serial.println("[CONFIG] Exemple: config-get pubnub_subscribe_key");
    #else
    Serial.println("[CONFIG] Exemple: config-get wifi_ssid");
    #endif
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
    #ifdef HAS_PUBNUB
    Serial.println("[CONFIG]   config-set pubnub_subscribe_key sub-c-xxx");
    Serial.println("[CONFIG]   config-set pubnub_publish_key pub-c-xxx");
    #endif
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
