#include "model_mqtt_routes.h"
#include "app_config.h"
#include "common/config/default_config.h"
#include "common/managers/led/led_manager.h"
#include "common/managers/init/init_manager.h"
#include "common/managers/wifi/wifi_manager.h"
#include "common/managers/mqtt/mqtt_manager.h"
#include "common/managers/sd/sd_manager.h"
#include "common/managers/rtc/rtc_manager.h"
#include "common/managers/timezone/timezone_manager.h"
#include <ArduinoJson.h>
#include <SD.h>
#include "../config/dream_config.h"
#include "common/managers/nfc/nfc_manager.h"
#include "common/managers/ota/ota_manager.h"
#include "common/utils/mac_utils.h"
#include "models/dream/managers/bedtime/bedtime_manager.h"
#include "models/dream/managers/wakeup/wakeup_manager.h"
#include "models/dream/managers/touch/dream_touch_handler.h"
#include "models/dream/utils/led_effect_parser.h"
#include "models/model_config.h"
#ifdef HAS_ENV_SENSOR
#include "common/managers/env_sensor/env_sensor_manager.h"
#endif
#include <limits.h>   // Pour ULONG_MAX
#include <math.h>     // isnan, isfinite
#ifdef HAS_WIFI
#include <HTTPClient.h>
#endif

/**
 * Routes MQTT spécifiques au modèle Kidoo Dream
 */

// Déclaration anticipée pour handleGetInfo (définition complète plus bas)
static bool testBedtimeActive = false;

bool ModelDreamMqttRoutes::processMessage(const JsonObject& json) {
  // Vérifier que l'action est présente
  if (!json["action"].is<const char*>()) {
    Serial.println("[MQTT-ROUTE] Erreur: action manquante dans le message");
    return false;
  }

  const char* action = json["action"].as<const char*>();
  if (action == nullptr) {
    Serial.println("[MQTT-ROUTE] Erreur: action est null");
    return false;
  }

  Serial.print("[MQTT-ROUTE] Traitement de l'action: ");
  Serial.println(action);

  // Router vers le bon handler
  if (strcmp(action, "get-info") == 0 || strcmp(action, "getinfo") == 0) {
    return handleGetInfo(json);
  }
  else if (strcmp(action, "brightness") == 0) {
    return handleBrightness(json);
  }
  else if (strcmp(action, "sleep-timeout") == 0 || strcmp(action, "sleeptimeout") == 0 || strcmp(action, "sleep") == 0) {
    return handleSleepTimeout(json);
  }
  else if (strcmp(action, "reboot") == 0 || strcmp(action, "restart") == 0) {
    return handleReboot(json);
  }
  else if (strcmp(action, "led") == 0) {
    return handleLed(json);
  }
  else if (strcmp(action, "start-test-bedtime") == 0) {
    return handleStartTestBedtime(json);
  }
  else if (strcmp(action, "stop-test-bedtime") == 0) {
    return handleStopTestBedtime(json);
  }
  else if (strcmp(action, "start-bedtime") == 0) {
    return handleStartBedtime(json);
  }
  else if (strcmp(action, "stop-bedtime") == 0) {
    return handleStopBedtime(json);
  }
  else if (strcmp(action, "stop-routine") == 0) {
    return handleStopRoutine(json);
  }
  else if (strcmp(action, "set-bedtime-config") == 0) {
    return handleSetBedtimeConfig(json);
  }
  else if (strcmp(action, "start-test-wakeup") == 0) {
    return handleStartTestWakeup(json);
  }
  else if (strcmp(action, "stop-test-wakeup") == 0) {
    return handleStopTestWakeup(json);
  }
  else if (strcmp(action, "set-wakeup-config") == 0) {
    return handleSetWakeupConfig(json);
  }
  else if (strcmp(action, "firmware-update") == 0) {
    return handleFirmwareUpdate(json);
  }
  else if (strcmp(action, "get-env") == 0 || strcmp(action, "getenv") == 0 || strcmp(action, "env") == 0) {
    return handleGetEnv(json);
  }
  else if (strcmp(action, "set-nighttime-alert") == 0) {
    return handleSetNighttimeAlert(json);
  }
  else if (strcmp(action, "nighttime-alert-ack") == 0) {
    return handleNighttimeAlertAck(json);
  }
  else if (strcmp(action, "set-default-config") == 0) {
    return handleSetDefaultConfig(json);
  }
  else if (strcmp(action, "start-test-default-config") == 0) {
    return handleStartTestDefaultConfig(json);
  }
  else if (strcmp(action, "stop-test-default-config") == 0) {
    return handleStopTestDefaultConfig(json);
  }
  else if (strcmp(action, "set-timezone") == 0) {
    return handleSetTimezone(json);
  }
  else if (strcmp(action, "tap-sensor") == 0) {
    Serial.println("[MQTT-ROUTE] tap-sensor: Simulation d'un tap");
    DreamTouchHandler::simulateTap();
    return true;
  }

  Serial.print("[MQTT-ROUTE] Action inconnue: ");
  Serial.println(action);
  return false;
}

bool ModelDreamMqttRoutes::handleGetInfo(const JsonObject& json) {
  // Format: { "action": "get-info" }
  // Publie les informations complètes de l'appareil (incluant données env si disponibles)

  Serial.println("[MQTT-ROUTE] get-info: Préparation des informations du Kidoo...");

  SDConfig config = SDManager::getConfig();

  // Récupérer les infos de stockage
  uint64_t totalBytes = 0;
  uint64_t freeBytes = 0;
  uint64_t usedBytes = 0;

  if (SDManager::isAvailable()) {
    totalBytes = SDManager::getTotalSpace();
    usedBytes = SDManager::getUsedSpace();
    freeBytes = SDManager::getFreeSpace();
  }

  // Récupérer l'adresse MAC WiFi (utilisée pour MQTT)
  // Sur ESP32-C3, BLE et WiFi ont des adresses MAC différentes
  char macStr[18];
  if (!getMacAddressString(macStr, sizeof(macStr), ESP_MAC_WIFI_STA)) {
    strcpy(macStr, "00:00:00:00:00:00"); // Valeur par défaut en cas d'erreur
  }

  // État courant du device (Dream: bedtime, wakeup, idle, manual) pour get-info
  // "manual" = routine démarrée manuellement (app ou tap) OU test en cours (config sauvegardée) OU couleur par défaut affichée par tap
  const char* deviceState = "idle";
  if (BedtimeManager::isBedtimeActive()) {
    deviceState = BedtimeManager::isManuallyStarted() ? "manual" : "bedtime";
  } else if (WakeupManager::isWakeupActive()) {
    deviceState = "wakeup";
  } else if (testBedtimeActive) {
    // Test bedtime actif (après sauvegarde config) → affichage "Manuel" dans l'app
    deviceState = "manual";
  } else if (DreamTouchHandler::isDefaultColorDisplayed()) {
    // Couleur par défaut affichée (via tap sans routine) → affichage "Manuel" dans l'app
    deviceState = "manual";
  }
  Serial.printf("[MQTT-ROUTE] get-info: deviceState=%s (bedtimeActive=%d, manuallyStarted=%d, testBedtimeActive=%d, defaultColorDisplayed=%d)\n",
    deviceState, BedtimeManager::isBedtimeActive(), BedtimeManager::isManuallyStarted(), testBedtimeActive, DreamTouchHandler::isDefaultColorDisplayed());

  // Préparer les données env (temperature, humidity, pressure)
  char envJson[300] = "";
#ifdef HAS_ENV_SENSOR
  if (EnvSensorManager::isInitialized() && EnvSensorManager::isAvailable()) {
    float t = EnvSensorManager::getTemperatureC();
    float h = EnvSensorManager::getHumidityPercent();
    float p = EnvSensorManager::getPressurePa();

    // Format JSON garanti (évite locale/notation scientifique qui peut invalider le JSON)
    char tStr[16] = {0}, hStr[16] = {0}, pStr[16] = {0};

    // Température
    if (!isfinite(t) || isnan(t) || t < -50.0f || t > 150.0f) {
      snprintf(tStr, sizeof(tStr), "null");
    } else {
      int ti = (int)t;
      int td = (int)((t - (float)ti) * 10);
      if (td < 0) td = -td;
      snprintf(tStr, sizeof(tStr), "%d.%d", ti, td);
    }

    // Humidité
    if (!isfinite(h) || isnan(h) || h < 0.0f || h > 100.0f) {
      snprintf(hStr, sizeof(hStr), "null");
    } else {
      int hi = (int)h;
      int hd = (int)((h - (float)hi) * 10);
      if (hd < 0) hd = -hd;
      snprintf(hStr, sizeof(hStr), "%d.%d", hi, hd);
    }

    // Pression
    if (!isfinite(p) || isnan(p) || p < 10000.0f || p > 120000.0f) {
      snprintf(pStr, sizeof(pStr), "null");
    } else {
      snprintf(pStr, sizeof(pStr), "%d", (int)p);
    }

    snprintf(envJson, sizeof(envJson),
      ",\"env\":{\"available\":true,\"temperatureC\":%s,\"humidityPercent\":%s,\"pressurePa\":%s}",
      tStr, hStr, pStr);
  } else {
    strcpy(envJson, ",\"env\":{\"available\":false}");
  }
#else
  strcpy(envJson, ",\"env\":{\"available\":false}");
#endif

  // Construire le JSON de réponse (700 bytes suffisent pour info + env data)
  char infoJson[700];
  snprintf(infoJson, sizeof(infoJson),
    "{"
      "\"type\":\"info\","
      "\"device\":\"%s\","
      "\"mac\":\"%s\","
      "\"ip\":\"%s\","
      "\"model\":\"dream\","
      "\"firmwareVersion\":\"%s\","
      "\"uptime\":%lu,"
      "\"freeHeap\":%u,"
      "\"wifi\":{"
        "\"ssid\":\"%s\","
        "\"rssi\":%d"
      "},"
      "\"brightness\":%d,"
      "\"sleepTimeout\":%lu,"
      "\"storage\":{"
        "\"total\":%llu,"
        "\"free\":%llu,"
        "\"used\":%llu"
      "},"
      "\"nfc\":{"
        "\"available\":%s"
      "},"
      "\"deviceState\":\"%s\"%s"
    "}",
    DEFAULT_DEVICE_NAME,
    macStr,
    WiFiManager::getLocalIP().c_str(),
    FIRMWARE_VERSION,
    millis() / 1000,
    ESP.getFreeHeap(),
    config.wifi_ssid,
    WiFiManager::getRSSI(),
    (config.led_brightness * 100 + 127) / 255,
    config.sleep_timeout_ms,
    totalBytes,
    freeBytes,
    usedBytes,
    NFCManager::isAvailable() ? "true" : "false",
    deviceState,
    envJson
  );

  if (MqttManager::publish(infoJson)) {
    Serial.println("[MQTT-ROUTE] get-info: Informations publiees avec succes");
  } else {
    Serial.println("[MQTT-ROUTE] get-info: Erreur lors de la publication des informations");
  }

  return true;
}

bool ModelDreamMqttRoutes::handleBrightness(const JsonObject& json) {
  // Format: { "action": "brightness", "params": { "value": 0-100 } }
  // Ou legacy: { "action": "brightness", "value": 0-100 }
  
  int value = -1;
  
  // Nouvelle syntaxe avec params
  if (json["params"].is<JsonObject>()) {
    JsonObject params = json["params"].as<JsonObject>();
    if (params["value"].is<int>()) {
      value = params["value"].as<int>();
    }
  }
  // Legacy: value directement dans le message
  else if (json["value"].is<int>()) {
    value = json["value"].as<int>();
  }
  
  if (value < 0) {
    Serial.println("[MQTT-ROUTE] brightness: parametre 'value' manquant");
    return false;
  }
  
  // Valider la plage (0-100%)
  if (value < 0) value = 0;
  if (value > 100) value = 100;
  
  // Convertir en 0-255 avec arrondi correct (0% → 0, 50% → 127, 100% → 255)
  uint8_t brightness = LEDManager::brightnessPercentTo255((uint8_t)value);
  
  if (LEDManager::setBrightness(brightness)) {
    Serial.print("[MQTT-ROUTE] Luminosite: ");
    Serial.print(value);
    Serial.println("%");
    
    // Sauvegarder dans la config
    SDConfig config = SDManager::getConfig();
    config.led_brightness = brightness;
    SDManager::saveConfig(config);
    
    return true;
  }
  
  return false;
}

bool ModelDreamMqttRoutes::handleSleepTimeout(const JsonObject& json) {
  // Format: { "action": "sleep-timeout", "params": { "value": 30000 } }
  // Ou legacy: { "action": "sleep", "timeout": 30000 }
  // value/timeout en ms, 0 = désactivé
  
  int32_t timeout = -1;
  
  // Nouvelle syntaxe avec params
  if (json["params"].is<JsonObject>()) {
    JsonObject params = json["params"].as<JsonObject>();
    if (params["value"].is<int>()) {
      timeout = params["value"].as<int>();
    }
  }
  // Legacy: timeout directement dans le message
  else if (json["timeout"].is<int>()) {
    timeout = json["timeout"].as<int>();
  }
  // Legacy: enabled = false
  else if (json["enabled"].is<bool>() && !json["enabled"].as<bool>()) {
    timeout = 0;
  }
  
  if (timeout < 0) {
    Serial.println("[MQTT-ROUTE] sleep-timeout: parametre 'value' ou 'timeout' manquant");
    return false;
  }
  
  // Minimum 5 secondes si activé
  if (timeout > 0 && timeout < 5000) {
    timeout = 5000;
  }
  
  // Maximum 5 minutes
  if (timeout > 300000) {
    timeout = 300000;
  }
  
  SDConfig config = SDManager::getConfig();
  config.sleep_timeout_ms = timeout;
  SDManager::saveConfig(config);
  
  if (timeout == 0) {
    Serial.println("[MQTT-ROUTE] Sleep mode desactive");
  } else {
    Serial.print("[MQTT-ROUTE] Sleep timeout: ");
    Serial.print(timeout);
    Serial.println(" ms");
  }
  
  return true;
}

bool ModelDreamMqttRoutes::handleReboot(const JsonObject& json) {
  // Format: { "action": "reboot", "params": { "delay": 1000 } }
  // delay en ms (optionnel, défaut = 0)
  
  uint32_t delayMs = 0;
  
  // Nouvelle syntaxe avec params
  if (json["params"].is<JsonObject>()) {
    JsonObject params = json["params"].as<JsonObject>();
    if (params["delay"].is<int>()) {
      delayMs = params["delay"].as<int>();
    }
  }
  // Legacy: delay directement dans le message
  else if (json["delay"].is<int>()) {
    delayMs = json["delay"].as<int>();
  }
  
  if (delayMs > 0) {
    Serial.print("[MQTT-ROUTE] Redemarrage dans ");
    Serial.print(delayMs);
    Serial.println(" ms");
    delay(delayMs);
  } else {
    Serial.println("[MQTT-ROUTE] Redemarrage immediat");
  }
  
  ESP.restart();
  
  return true;  // Ne sera jamais atteint
}

bool ModelDreamMqttRoutes::handleLed(const JsonObject& json) {
  // Format: { "action": "led", "color": "#FF0000", "effect": "solid" }
  // color: hex string (#RRGGBB) ou nom (red, green, blue, etc.)
  // effect: none, pulse, rotate, rainbow, glossy
  
  bool handled = false;
  
  // Traiter la couleur
  if (json["color"].is<const char*>()) {
    const char* colorStr = json["color"].as<const char*>();
    uint8_t r = 0, g = 0, b = 0;
    
    if (colorStr[0] == '#' && strlen(colorStr) == 7) {
      // Format hex #RRGGBB
      long hexColor = strtol(colorStr + 1, nullptr, 16);
      r = (hexColor >> 16) & 0xFF;
      g = (hexColor >> 8) & 0xFF;
      b = hexColor & 0xFF;
    }
    else if (strcmp(colorStr, "red") == 0) { r = 255; g = 0; b = 0; }
    else if (strcmp(colorStr, "green") == 0) { r = 0; g = 255; b = 0; }
    else if (strcmp(colorStr, "blue") == 0) { r = 0; g = 0; b = 255; }
    else if (strcmp(colorStr, "white") == 0) { r = 255; g = 255; b = 255; }
    else if (strcmp(colorStr, "yellow") == 0) { r = 255; g = 255; b = 0; }
    else if (strcmp(colorStr, "cyan") == 0) { r = 0; g = 255; b = 255; }
    else if (strcmp(colorStr, "magenta") == 0) { r = 255; g = 0; b = 255; }
    else if (strcmp(colorStr, "orange") == 0) { r = 255; g = 165; b = 0; }
    else if (strcmp(colorStr, "purple") == 0) { r = 128; g = 0; b = 128; }
    else if (strcmp(colorStr, "pink") == 0) { r = 255; g = 192; b = 203; }
    // off/black = 0,0,0 (déjà initialisé)
    
    LEDManager::setColor(r, g, b);
    Serial.print("[MQTT-ROUTE] Couleur: ");
    Serial.println(colorStr);
    handled = true;
  }
  
  // Traiter l'effet
  if (json["effect"].is<const char*>()) {
    const char* effectStr = json["effect"].as<const char*>();

    if (strcmp(effectStr, "off") == 0) {
      // Éteindre les LEDs
      LEDManager::clear();
      Serial.println("[MQTT-ROUTE] LEDs eteintes");
      return true;
    }

    LEDEffect effect = LEDEffectParser::parse(effectStr);
    LEDManager::setEffect(effect);
    Serial.print("[MQTT-ROUTE] Effet: ");
    Serial.println(effectStr);
    handled = true;
  }
  
  if (!handled) {
    Serial.println("[MQTT-ROUTE] led: parametre 'color' ou 'effect' manquant");
  }
  
  return handled;
}

// Variables statiques pour gérer l'état du test de bedtime (testBedtimeActive déclaré en haut)
static unsigned long testBedtimeStartTime = 0;
static bool bedtimeWasActiveBeforeTest = false;  // Pour restaurer le mode bedtime à la sortie du test
static bool testWakeupActive = false;
static unsigned long testWakeupStartTime = 0;
static const unsigned long TEST_BEDTIME_TIMEOUT_MS = 15000; // 15 secondes

// "J'arrive" : signal envoyé par l'app quand le parent tape sur la notification d'alerte nocturne
static bool nighttimeAlertAckActive = false;
static unsigned long nighttimeAlertAckStartTime = 0;
static bool nighttimeAlertAckBedtimeWasActive = false;
static const unsigned long NIGHTTIME_ALERT_ACK_DURATION_MS = 5000; // 5 secondes de rotate rainbow

static bool testDefaultConfigActive = false;
static unsigned long testDefaultConfigStartTime = 0;
static const unsigned long TEST_DEFAULT_CONFIG_TIMEOUT_MS = 15000; // 15 secondes

bool ModelDreamMqttRoutes::handleStartTestBedtime(const JsonObject& json) {
  // Format: { "action": "start-test-bedtime", "params": { "colorR": 255, "colorG": 107, "colorB": 107, "brightness": 50 } }
  // Démarre le test de la couleur et luminosité avec les paramètres fournis
  
  Serial.println("[MQTT-ROUTE] start-test-bedtime: Démarrage/mise à jour du test...");
  
  // Si c'est un nouveau test (pas une mise à jour), mémoriser si le bedtime était actif pour restaurer à la sortie
  if (!testBedtimeActive) {
    bedtimeWasActiveBeforeTest = BedtimeManager::isBedtimeActive();
  }
  // Sauvegarder l'état actif pour savoir si c'est une mise à jour ou un nouveau test
  bool wasAlreadyActive = testBedtimeActive;
  
  // Récupérer les paramètres
  int colorR = -1;
  int colorG = -1;
  int colorB = -1;
  int brightness = -1;
  const char* effectStr = nullptr;
  bool hasEffect = false;
  
  Serial.print("[MQTT-ROUTE] start-test-bedtime: Message JSON reçu - ");
  serializeJson(json, Serial);
  Serial.println();
  
  // Nouvelle syntaxe avec params
  if (json["params"].is<JsonObject>()) {
    JsonObject params = json["params"].as<JsonObject>();
    Serial.println("[MQTT-ROUTE] start-test-bedtime: Utilisation de la syntaxe avec params");
    if (params["colorR"].is<int>()) colorR = params["colorR"].as<int>();
    if (params["colorG"].is<int>()) colorG = params["colorG"].as<int>();
    if (params["colorB"].is<int>()) colorB = params["colorB"].as<int>();
    if (params["brightness"].is<int>()) brightness = params["brightness"].as<int>();
    if (params["effect"].is<const char*>()) {
      effectStr = params["effect"].as<const char*>();
      hasEffect = true;
    }
    
    Serial.print("[MQTT-ROUTE] start-test-bedtime: Paramètres extraits - RGB(");
    Serial.print(colorR);
    Serial.print(",");
    Serial.print(colorG);
    Serial.print(",");
    Serial.print(colorB);
    Serial.print("), Brightness: ");
    Serial.println(brightness);
  }
  // Syntaxe directe (legacy)
  else {
    Serial.println("[MQTT-ROUTE] start-test-bedtime: Utilisation de la syntaxe directe (legacy)");
    if (json["colorR"].is<int>()) colorR = json["colorR"].as<int>();
    if (json["colorG"].is<int>()) colorG = json["colorG"].as<int>();
    if (json["colorB"].is<int>()) colorB = json["colorB"].as<int>();
    if (json["brightness"].is<int>()) brightness = json["brightness"].as<int>();
    if (json["effect"].is<const char*>()) {
      effectStr = json["effect"].as<const char*>();
      hasEffect = true;
    }
  }
  
  // Valider les paramètres
  if (colorR < 0 || colorR > 255 || colorG < 0 || colorG > 255 || colorB < 0 || colorB > 255) {
    Serial.println("[MQTT-ROUTE] start-test-bedtime: Couleur invalide");
    return false;
  }
  
  if (brightness < 0 || brightness > 100) {
    Serial.println("[MQTT-ROUTE] start-test-bedtime: Brightness invalide");
    return false;
  }
  
  // Convertir brightness de 0-100 vers 0-255
  uint8_t brightnessValue = (brightness * 255 + 50) / 100;
  
  // Sortir du mode sommeil avant d'appliquer le test
  // (setColor et setBrightness appellent déjà wakeUp(), mais on le fait explicitement pour être sûr)
  LEDManager::wakeUp();
  
  // Convertir l'effet string en enum LEDEffect si fourni
  LEDEffect effect = LED_EFFECT_NONE;
  if (hasEffect && effectStr != nullptr) {
    effect = LEDEffectParser::parse(effectStr);
  }
  
  // Appliquer les paramètres du test
  // Ordre : d'abord appliquer l'effet (ou NONE si pas d'effet),
  // puis appliquer la couleur et la brightness
  // Note: setEffect peut réinitialiser la couleur à noir si on change d'effet,
  // donc on doit réappliquer la couleur après
  LEDManager::setEffect(effect); // Appliquer l'effet configuré (ou NONE)
  LEDManager::setColor(colorR, colorG, colorB); // Appliquer la couleur (après setEffect pour qu'elle soit préservée)
  LEDManager::setBrightness(brightnessValue); // Appliquer la brightness
  
  // Activer le test (ou le maintenir actif si déjà actif)
  testBedtimeActive = true;
  
  // TOUJOURS réinitialiser le timer après avoir validé et appliqué les paramètres
  // Cela permet de réinitialiser le timeout de 15s à chaque mise à jour (changement de couleur/brightness)
  testBedtimeStartTime = millis();
  if (wasAlreadyActive) {
    Serial.println("[MQTT-ROUTE] start-test-bedtime: Test déjà actif, timeout de 15 secondes réinitialisé");
  } else {
    Serial.println("[MQTT-ROUTE] start-test-bedtime: Nouveau test démarré, timeout de 15 secondes initialisé");
  }
  
  Serial.print("[MQTT-ROUTE] start-test-bedtime: Test démarré - Couleur RGB(");
  Serial.print(colorR);
  Serial.print(",");
  Serial.print(colorG);
  Serial.print(",");
  Serial.print(colorB);
  Serial.print("), Brightness: ");
  Serial.print(brightness);
  Serial.print("%");
  if (hasEffect && effectStr != nullptr) {
    Serial.print(", Effect: ");
    Serial.print(effectStr);
  }
  Serial.println();
  
  return true;
}

bool ModelDreamMqttRoutes::handleStartBedtime(const JsonObject& json) {
  // Format: { "action": "start-bedtime" }
  // Démarre manuellement la routine de coucher avec la configuration sauvegardée
  // Empêche le déclenchement automatique programmé
  
  Serial.println("[MQTT-ROUTE] start-bedtime: Démarrage manuel de la routine de coucher");
  
  // Vérifier que BedtimeManager est initialisé (config chargée)
  // Pas besoin de schedule activé : le manuel fonctionne même sans horaire programmé
  if (!BedtimeManager::init()) {
    Serial.println("[MQTT-ROUTE] start-bedtime: ERREUR - BedtimeManager non initialisé");
    return false;
  }
  
  // Démarrer (ou forcer le redémarrage) du bedtime selon la config
  BedtimeManager::startBedtimeManually();
  
  Serial.printf("[MQTT-ROUTE] start-bedtime: OK - bedtimeActive=%d manuallyStarted=%d\n",
    BedtimeManager::isBedtimeActive(), BedtimeManager::isManuallyStarted());
  return true;
}

bool ModelDreamMqttRoutes::handleStopBedtime(const JsonObject& json) {
  // Format: { "action": "stop-bedtime" }
  // Arrête manuellement la routine de coucher
  
  Serial.println("[MQTT-ROUTE] stop-bedtime: Arrêt manuel de la routine de coucher");
  
  // Vérifier si le bedtime est actif
  if (!BedtimeManager::isBedtimeActive()) {
    Serial.println("[MQTT-ROUTE] stop-bedtime: Aucun bedtime actif");
    return false;
  }
  
  // Arrêter le bedtime manuellement
  BedtimeManager::stopBedtimeManually();
  
  Serial.println("[MQTT-ROUTE] stop-bedtime: Routine de coucher arrêtée manuellement");
  return true;
}

bool ModelDreamMqttRoutes::handleStopRoutine(const JsonObject& json) {
  // Format: { "action": "stop-routine" }
  // Arrête la routine active (bedtime ou wakeup)
  
  Serial.println("[MQTT-ROUTE] stop-routine: Arrêt de la routine active");
  
  bool stopped = false;
  
  // Vérifier si le bedtime est actif
  if (BedtimeManager::isBedtimeActive()) {
    Serial.println("[MQTT-ROUTE] stop-routine: Arrêt du bedtime actif");
    BedtimeManager::stopBedtimeManually();
    stopped = true;
  }
  
  // Vérifier si le wakeup est actif
  if (WakeupManager::isWakeupActive()) {
    Serial.println("[MQTT-ROUTE] stop-routine: Arrêt du wakeup actif");
    WakeupManager::stopWakeupManually();
    stopped = true;
  }
  
  if (!stopped) {
    Serial.println("[MQTT-ROUTE] stop-routine: Aucune routine active");
    return false;
  }
  
  Serial.println("[MQTT-ROUTE] stop-routine: Routine arrêtée");
  return true;
}

bool ModelDreamMqttRoutes::handleStopTestBedtime(const JsonObject& json) {
  // Format: { "action": "stop-test-bedtime" }
  // Arrête le test de l'heure de coucher
  
  if (!testBedtimeActive) {
    Serial.println("[MQTT-ROUTE] stop-test-bedtime: Aucun test actif");
    return false;
  }
  
  Serial.println("[MQTT-ROUTE] stop-test-bedtime: Arrêt du test");
  
  // Désactiver le test avant de restaurer (pour que restoreDisplayFromConfig voie l'état cohérent)
  testBedtimeActive = false;
  testBedtimeStartTime = 0;
  
  // Si on était en horaire bedtime avant le test, revenir au mode bedtime (config)
  if (bedtimeWasActiveBeforeTest) {
    bedtimeWasActiveBeforeTest = false;
    BedtimeManager::restoreDisplayFromConfig();
  } else {
    LEDManager::clear();
  }
  
  return true;
}

void ModelDreamMqttRoutes::checkTestBedtimeTimeout() {
  // Vérifier si le timeout est dépassé
  // Utiliser une comparaison sécurisée pour gérer le wrap-around de millis()
  if (testBedtimeActive) {
    unsigned long currentTime = millis();
    unsigned long elapsedTime;
    
    // Gérer le wrap-around de millis() (se produit après ~49 jours)
    if (currentTime >= testBedtimeStartTime) {
      elapsedTime = currentTime - testBedtimeStartTime;
    } else {
      // Wrap-around détecté
      elapsedTime = (ULONG_MAX - testBedtimeStartTime) + currentTime;
    }
    
    if (elapsedTime >= TEST_BEDTIME_TIMEOUT_MS) {
      Serial.println("[MQTT-ROUTE] Test bedtime: Timeout de 15 secondes atteint, arrêt automatique");
      
      // Créer un JsonObject vide pour appeler handleStopTestBedtime
      #pragma GCC diagnostic push
      #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
      StaticJsonDocument<1> doc;
      #pragma GCC diagnostic pop
      JsonObject emptyJson = doc.to<JsonObject>();
      handleStopTestBedtime(emptyJson);
    }
  }
}

void ModelDreamMqttRoutes::checkTestDefaultConfigTimeout() {
  if (testDefaultConfigActive) {
    unsigned long currentTime = millis();
    unsigned long elapsedTime;
    if (currentTime >= testDefaultConfigStartTime) {
      elapsedTime = currentTime - testDefaultConfigStartTime;
    } else {
      elapsedTime = (ULONG_MAX - testDefaultConfigStartTime) + currentTime;
    }
    if (elapsedTime >= TEST_DEFAULT_CONFIG_TIMEOUT_MS) {
      Serial.println("[MQTT-ROUTE] Test default-config: Timeout de 15 secondes atteint, arrêt automatique");
      #pragma GCC diagnostic push
      #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
      StaticJsonDocument<1> doc;
      #pragma GCC diagnostic pop
      JsonObject emptyJson = doc.to<JsonObject>();
      handleStopTestDefaultConfig(emptyJson);
    }
  }
}

void ModelDreamMqttRoutes::checkTestWakeupTimeout() {
  // Vérifier si le timeout est dépassé
  // Utiliser une comparaison sécurisée pour gérer le wrap-around de millis()
  if (testWakeupActive) {
    unsigned long currentTime = millis();
    unsigned long elapsedTime;
    
    // Gérer le wrap-around de millis() (se produit après ~49 jours)
    if (currentTime >= testWakeupStartTime) {
      elapsedTime = currentTime - testWakeupStartTime;
    } else {
      // Wrap-around détecté
      elapsedTime = (ULONG_MAX - testWakeupStartTime) + currentTime;
    }
    
    if (elapsedTime >= 15000) { // 15 secondes
      // Timeout dépassé, arrêter le test
      Serial.println("[MQTT-ROUTE] checkTestWakeupTimeout: Timeout de 15 secondes dépassé, arrêt du test");
      
      // Créer un JsonObject vide pour appeler handleStopTestWakeup
      #pragma GCC diagnostic push
      #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
      StaticJsonDocument<16> emptyDoc;
      #pragma GCC diagnostic pop
      JsonObject emptyJson = emptyDoc.to<JsonObject>();
      
      handleStopTestWakeup(emptyJson);
    }
  }
}

bool ModelDreamMqttRoutes::isTestWakeupActive() {
  return testWakeupActive;
}

bool ModelDreamMqttRoutes::isTestBedtimeActive() {
  return testBedtimeActive;
}

void ModelDreamMqttRoutes::resetTestFlags() {
  testBedtimeActive = false;
  testBedtimeStartTime = 0;
  bedtimeWasActiveBeforeTest = false;
  testWakeupActive = false;
  testWakeupStartTime = false;
  testDefaultConfigActive = false;
  testDefaultConfigStartTime = 0;
}

bool ModelDreamMqttRoutes::handleNighttimeAlertAck(const JsonObject& json) {
  // Format: { "action": "nighttime-alert-ack" }
  // Envoyé par l'app quand le parent tape "J'arrive" sur la notification d'alerte nocturne.
  // Joue l'effet rainbow pendant 5 secondes pour notifier l'enfant que quelqu'un arrive.
  
  Serial.println("[MQTT-ROUTE] nighttime-alert-ack: Signal J'arrive reçu, rotate rainbow 5 secondes");

  // Mémoriser si on était en bedtime pour restaurer à la fin
  if (!nighttimeAlertAckActive) {
    nighttimeAlertAckBedtimeWasActive = BedtimeManager::isBedtimeActive();
  }

  LEDManager::preventSleep();
  // NOTE: Pas de wakeUp() car cela déclenche une animation de fade-in qui flashe l'écran
  // setEffect() allume déjà les LEDs. Si le device est en sleep mode, il se réveillera automatiquement.
  LEDManager::setEffect(LED_EFFECT_RAINBOW);
  LEDManager::setBrightness(LEDManager::brightnessPercentTo255(80));  // Luminosité visible
  
  nighttimeAlertAckActive = true;
  nighttimeAlertAckStartTime = millis();
  
  return true;
}

void ModelDreamMqttRoutes::checkNighttimeAlertAckTimeout() {
  if (!nighttimeAlertAckActive) return;
  
  unsigned long currentTime = millis();
  unsigned long elapsedTime;
  if (currentTime >= nighttimeAlertAckStartTime) {
    elapsedTime = currentTime - nighttimeAlertAckStartTime;
  } else {
    elapsedTime = (ULONG_MAX - nighttimeAlertAckStartTime) + currentTime;
  }
  
  if (elapsedTime >= NIGHTTIME_ALERT_ACK_DURATION_MS) {
    Serial.println("[MQTT-ROUTE] nighttime-alert-ack: Fin du rotate rainbow, restauration affichage");
    nighttimeAlertAckActive = false;
    nighttimeAlertAckStartTime = 0;

    if (nighttimeAlertAckBedtimeWasActive) {
      nighttimeAlertAckBedtimeWasActive = false;
      BedtimeManager::restoreDisplayFromConfig();
    } else if (DreamTouchHandler::isDefaultColorDisplayed()) {
      // Restaurer la couleur par défaut après le "J'arrive"
      DreamConfig dreamConfig = DreamConfigManager::getConfig();
      LEDManager::preventSleep();
      LEDManager::wakeUp();
      // Nettoyer d'abord pour éviter les animations résiduelles du rainbow
      LEDManager::clear();
      delay(50);
      LEDManager::setColor(dreamConfig.default_color_r, dreamConfig.default_color_g, dreamConfig.default_color_b);
      LEDManager::setBrightness(LEDManager::brightnessPercentTo255(dreamConfig.default_brightness));
      LEDEffect defaultEffect = LEDEffectParser::parse(dreamConfig.default_effect);
      LEDManager::setEffect(defaultEffect);
    } else {
      LEDManager::clear();
    }
  }
}

bool ModelDreamMqttRoutes::handleSetDefaultConfig(const JsonObject& json) {
  // Format: { "action": "set-default-config", "colorR": 255, "colorG": 107, "colorB": 200, "brightness": 50, "effect": "rainbow_soft" }
  // Sauvegarde la configuration de couleur/effet par défaut pour le tap sans routine

  Serial.println("[MQTT-ROUTE] set-default-config: Sauvegarde de la configuration par défaut...");

  if (!SDManager::isAvailable()) {
    Serial.println("[MQTT-ROUTE] set-default-config: Carte SD non disponible");
    return false;
  }

  // Extraire les paramètres
  int colorR = -1;
  int colorG = -1;
  int colorB = -1;
  int brightness = -1;
  const char* effectStr = nullptr;
  bool hasEffect = false;

  // Les paramètres sont à la racine du JSON (pas dans un wrapper "params")
  if (json["colorR"].is<int>()) colorR = json["colorR"].as<int>();
  else if (json["colorR"].is<double>()) colorR = (int)json["colorR"].as<double>();
  if (json["colorG"].is<int>()) colorG = json["colorG"].as<int>();
  else if (json["colorG"].is<double>()) colorG = (int)json["colorG"].as<double>();
  if (json["colorB"].is<int>()) colorB = json["colorB"].as<int>();
  else if (json["colorB"].is<double>()) colorB = (int)json["colorB"].as<double>();
  if (json["brightness"].is<int>()) brightness = json["brightness"].as<int>();
  else if (json["brightness"].is<double>()) brightness = (int)json["brightness"].as<double>();

  if (json["effect"].is<const char*>()) {
    effectStr = json["effect"].as<const char*>();
    hasEffect = true;
  }

  // Validation
  if (colorR < 0 || colorR > 255 || colorG < 0 || colorG > 255 || colorB < 0 || colorB > 255) {
    Serial.println("[MQTT-ROUTE] set-default-config: Couleur invalide");
    return false;
  }

  if (brightness < 0 || brightness > 100) {
    Serial.println("[MQTT-ROUTE] set-default-config: Brightness invalide");
    return false;
  }

  // Charger la configuration actuelle
  DreamConfig dreamConfig = DreamConfigManager::getConfig();

  // Mettre à jour les champs
  dreamConfig.default_color_r = (uint8_t)colorR;
  dreamConfig.default_color_g = (uint8_t)colorG;
  dreamConfig.default_color_b = (uint8_t)colorB;
  dreamConfig.default_brightness = (uint8_t)brightness;

  // Sauvegarder l'effet si présent
  if (hasEffect && effectStr != nullptr) {
    strncpy(dreamConfig.default_effect, effectStr, sizeof(dreamConfig.default_effect) - 1);
    dreamConfig.default_effect[sizeof(dreamConfig.default_effect) - 1] = '\0';
  } else {
    // Pas d'effet = couleur unie
    strcpy(dreamConfig.default_effect, "");
  }

  // Sauvegarder sur la SD
  if (DreamConfigManager::saveConfig(dreamConfig)) {
    Serial.print("[MQTT-ROUTE] set-default-config: Configuration sauvegardée - RGB(");
    Serial.print(dreamConfig.default_color_r);
    Serial.print(",");
    Serial.print(dreamConfig.default_color_g);
    Serial.print(",");
    Serial.print(dreamConfig.default_color_b);
    Serial.print("), Brightness: ");
    Serial.print(dreamConfig.default_brightness);
    Serial.print("%, Effect: ");
    Serial.println(dreamConfig.default_effect);

    // Déclencher automatiquement le test avec la nouvelle configuration (comme set-bedtime-config)
    Serial.println("[MQTT-ROUTE] set-default-config: Déclenchement automatique du test...");
    // Directement appeler le test sans JsonDocument pour éviter débordement de stack
    LEDManager::wakeUp();
    LEDEffect effect = LED_EFFECT_NONE;
    if (hasEffect && effectStr != nullptr) {
      effect = LEDEffectParser::parse(effectStr);
    }
    LEDManager::setEffect(effect);
    LEDManager::setColor(colorR, colorG, colorB);
    uint8_t brightnessValue = (brightness * 255 + 50) / 100;
    LEDManager::setBrightness(brightnessValue);
    testDefaultConfigActive = true;
    testDefaultConfigStartTime = millis();
    Serial.println("[MQTT-ROUTE] set-default-config: Test automatique démarré avec succès");

    return true;
  } else {
    Serial.println("[MQTT-ROUTE] set-default-config: Erreur lors de la sauvegarde");
    return false;
  }
}

bool ModelDreamMqttRoutes::handleStartTestDefaultConfig(const JsonObject& json) {
  // Format: { "action": "start-test-default-config", "params": { "colorR": 255, "colorG": 107, "colorB": 200, "brightness": 50, "effect": "rainbow_soft" } }
  // Affiche la couleur/effet par défaut en temps réel sur l'ESP (comme start-test-bedtime)

  Serial.println("[MQTT-ROUTE] start-test-default-config: Démarrage/mise à jour du test...");

  bool wasAlreadyActive = testDefaultConfigActive;

  int colorR = -1;
  int colorG = -1;
  int colorB = -1;
  int brightness = -1;
  const char* effectStr = nullptr;
  bool hasEffect = false;

  if (json["params"].is<JsonObject>()) {
    JsonObject params = json["params"].as<JsonObject>();
    if (params["colorR"].is<int>()) colorR = params["colorR"].as<int>();
    else if (params["colorR"].is<double>()) colorR = (int)params["colorR"].as<double>();
    if (params["colorG"].is<int>()) colorG = params["colorG"].as<int>();
    else if (params["colorG"].is<double>()) colorG = (int)params["colorG"].as<double>();
    if (params["colorB"].is<int>()) colorB = params["colorB"].as<int>();
    else if (params["colorB"].is<double>()) colorB = (int)params["colorB"].as<double>();
    if (params["brightness"].is<int>()) brightness = params["brightness"].as<int>();
    else if (params["brightness"].is<double>()) brightness = (int)params["brightness"].as<double>();
    if (params["effect"].is<const char*>()) {
      effectStr = params["effect"].as<const char*>();
      hasEffect = true;
    }
  } else {
    if (json["colorR"].is<int>()) colorR = json["colorR"].as<int>();
    if (json["colorG"].is<int>()) colorG = json["colorG"].as<int>();
    if (json["colorB"].is<int>()) colorB = json["colorB"].as<int>();
    if (json["brightness"].is<int>()) brightness = json["brightness"].as<int>();
    if (json["effect"].is<const char*>()) {
      effectStr = json["effect"].as<const char*>();
      hasEffect = true;
    }
  }

  if (colorR < 0 || colorR > 255 || colorG < 0 || colorG > 255 || colorB < 0 || colorB > 255) {
    Serial.println("[MQTT-ROUTE] start-test-default-config: Couleur invalide");
    return false;
  }
  if (brightness < 0 || brightness > 100) {
    Serial.println("[MQTT-ROUTE] start-test-default-config: Brightness invalide");
    return false;
  }

  uint8_t brightnessValue = (brightness * 255 + 50) / 100;
  LEDManager::wakeUp();

  LEDEffect effect = LED_EFFECT_NONE;
  if (hasEffect && effectStr != nullptr) {
    effect = LEDEffectParser::parse(effectStr);
  }

  LEDManager::setEffect(effect);
  LEDManager::setColor(colorR, colorG, colorB);
  LEDManager::setBrightness(brightnessValue);

  testDefaultConfigActive = true;
  testDefaultConfigStartTime = millis();

  Serial.print("[MQTT-ROUTE] start-test-default-config: Test démarré - RGB(");
  Serial.print(colorR);
  Serial.print(",");
  Serial.print(colorG);
  Serial.print(",");
  Serial.print(colorB);
  Serial.print("), Brightness: ");
  Serial.print(brightness);
  Serial.print("%");
  if (hasEffect && effectStr != nullptr) {
    Serial.print(", Effect: ");
    Serial.print(effectStr);
  }
  Serial.println();

  return true;
}

bool ModelDreamMqttRoutes::handleStopTestDefaultConfig(const JsonObject& json) {
  if (!testDefaultConfigActive) {
    Serial.println("[MQTT-ROUTE] stop-test-default-config: Aucun test actif");
    return false;
  }
  Serial.println("[MQTT-ROUTE] stop-test-default-config: Arrêt du test");
  testDefaultConfigActive = false;
  testDefaultConfigStartTime = 0;
  LEDManager::clear();
  return true;
}

bool ModelDreamMqttRoutes::handleSetBedtimeConfig(const JsonObject& json) {
  // Format: { "action": "set-bedtime-config", "params": { "colorR": 255, "colorG": 107, "colorB": 107, "brightness": 50, "allNight": false, "weekdaySchedule": {...} } }
  // Sauvegarde la configuration de l'heure de coucher sur la carte SD
  
  Serial.println("[MQTT-ROUTE] set-bedtime-config: Sauvegarde de la configuration...");
  
  if (!SDManager::isAvailable()) {
    Serial.println("[MQTT-ROUTE] set-bedtime-config: Carte SD non disponible");
    return false;
  }
  
  // Récupérer les paramètres (le serveur envoie à la racine: action, colorR, weekdaySchedule, etc. — pas de wrapper "params")
  int colorR = -1;
  int colorG = -1;
  int colorB = -1;
  int brightness = -1;
  bool allNight = false;
  const char* effectStr = nullptr;
  bool hasEffect = false;
  JsonObject weekdayScheduleObj;
  String weekdayScheduleStr;
  bool hasWeekdaySchedule = false;
  JsonObject params = json["params"].is<JsonObject>() ? json["params"].as<JsonObject>() : json;

  if (params["colorR"].is<int>()) colorR = params["colorR"].as<int>();
  else if (params["colorR"].is<double>()) colorR = (int)params["colorR"].as<double>();
  if (params["colorG"].is<int>()) colorG = params["colorG"].as<int>();
  else if (params["colorG"].is<double>()) colorG = (int)params["colorG"].as<double>();
  if (params["colorB"].is<int>()) colorB = params["colorB"].as<int>();
  else if (params["colorB"].is<double>()) colorB = (int)params["colorB"].as<double>();
  if (params["brightness"].is<int>()) brightness = params["brightness"].as<int>();
  else if (params["brightness"].is<double>()) brightness = (int)params["brightness"].as<double>();
  if (params["allNight"].is<bool>()) allNight = params["allNight"].as<bool>();

  if (params["effect"].is<const char*>()) {
    effectStr = params["effect"].as<const char*>();
    hasEffect = true;
  }
  if (params["weekdaySchedule"].is<JsonObject>()) {
    weekdayScheduleObj = params["weekdaySchedule"].as<JsonObject>();
    hasWeekdaySchedule = true;
  } else if (params["weekdaySchedule"].is<const char*>()) {
    weekdayScheduleStr = params["weekdaySchedule"].as<const char*>();
    hasWeekdaySchedule = true;
  }
  
  // Valider les paramètres
  // Si un effet est fourni, la couleur n'est pas obligatoire (mais peut être fournie pour l'effet ROTATE)
  if (!hasEffect && (colorR < 0 || colorR > 255 || colorG < 0 || colorG > 255 || colorB < 0 || colorB > 255)) {
    Serial.println("[MQTT-ROUTE] set-bedtime-config: Couleur invalide (requise si pas d'effet)");
    return false;
  }
  
  if (brightness < 0 || brightness > 100) {
    Serial.println("[MQTT-ROUTE] set-bedtime-config: Brightness invalide");
    return false;
  }
  
  // Charger la configuration actuelle depuis la SD
  SDConfig config = SDManager::getConfig();
  
  // Mettre à jour les champs bedtime
  // Si une couleur est fournie, l'utiliser (même si un effet est aussi fourni, pour l'effet ROTATE)
  if (colorR >= 0 && colorR <= 255 && colorG >= 0 && colorG <= 255 && colorB >= 0 && colorB <= 255) {
    config.bedtime_colorR = (uint8_t)colorR;
    config.bedtime_colorG = (uint8_t)colorG;
    config.bedtime_colorB = (uint8_t)colorB;
  }
  config.bedtime_brightness = (uint8_t)brightness;
  config.bedtime_allNight = allNight;
  
  // Sauvegarder l'effet si présent
  if (hasEffect && effectStr != nullptr) {
    strncpy(config.bedtime_effect, effectStr, sizeof(config.bedtime_effect) - 1);
    config.bedtime_effect[sizeof(config.bedtime_effect) - 1] = '\0';
    Serial.print("[MQTT-ROUTE] set-bedtime-config: Effet sauvegardé: ");
    Serial.println(config.bedtime_effect);
  } else {
    // Pas d'effet fourni, utiliser "none" pour couleur fixe
    strcpy(config.bedtime_effect, "none");
  }
  
  // Sauvegarder weekdaySchedule si présent
  if (hasWeekdaySchedule) {
    String scheduleStr = weekdayScheduleStr.length() > 0 ? weekdayScheduleStr : String();
    if (scheduleStr.length() == 0) {
      serializeJson(weekdayScheduleObj, scheduleStr);
    }
    if (scheduleStr.length() < sizeof(config.bedtime_weekdaySchedule)) {
      strncpy(config.bedtime_weekdaySchedule, scheduleStr.c_str(), sizeof(config.bedtime_weekdaySchedule) - 1);
      config.bedtime_weekdaySchedule[sizeof(config.bedtime_weekdaySchedule) - 1] = '\0';
      Serial.print("[MQTT-ROUTE] set-bedtime-config: weekdaySchedule sauvegardé: ");
      Serial.println(config.bedtime_weekdaySchedule);
    } else {
      Serial.println("[MQTT-ROUTE] set-bedtime-config: weekdaySchedule trop grand, tronqué");
      strncpy(config.bedtime_weekdaySchedule, scheduleStr.c_str(), sizeof(config.bedtime_weekdaySchedule) - 1);
      config.bedtime_weekdaySchedule[sizeof(config.bedtime_weekdaySchedule) - 1] = '\0';
    }
  } else {
    if (strlen(config.bedtime_weekdaySchedule) == 0) {
      strcpy(config.bedtime_weekdaySchedule, "{}");
    }
  }
  
  // Sauvegarder sur la SD
  if (SDManager::saveConfig(config)) {
    Serial.print("[MQTT-ROUTE] set-bedtime-config: Configuration sauvegardée - RGB(");
    Serial.print(config.bedtime_colorR);
    Serial.print(",");
    Serial.print(config.bedtime_colorG);
    Serial.print(",");
    Serial.print(config.bedtime_colorB);
    Serial.print("), Brightness: ");
    Serial.print(brightness);
    Serial.print("%, AllNight: ");
    Serial.print(allNight ? "true" : "false");
    Serial.print(", Effect: ");
    Serial.print(config.bedtime_effect);
    if (hasWeekdaySchedule) {
      Serial.print(", weekdaySchedule: ");
      Serial.println(config.bedtime_weekdaySchedule);
    } else {
      Serial.println();
    }
    
    // Recharger la configuration dans le BedtimeManager
    BedtimeManager::reloadConfig();

    // Si on est en mode wakeup, ne pas lancer le test pour ne pas interrompre la routine
    if (WakeupManager::isWakeupActive()) {
      Serial.println("[MQTT-ROUTE] set-bedtime-config: Mode wakeup actif, test non lancé pour ne pas interrompre");
    } else {
      // Déclencher automatiquement le test avec la nouvelle configuration
      Serial.println("[MQTT-ROUTE] set-bedtime-config: Déclenchement automatique du test...");

      // Créer un JsonObject avec les paramètres pour le test
      // Utiliser les valeurs qui viennent d'être sauvegardées dans la config
      JsonDocument testJson;
      JsonObject testParams = testJson["params"].to<JsonObject>();
      testParams["colorR"] = config.bedtime_colorR;
      testParams["colorG"] = config.bedtime_colorG;
      testParams["colorB"] = config.bedtime_colorB;
      testParams["brightness"] = config.bedtime_brightness;

      // Ajouter l'effet si configuré
      if (strlen(config.bedtime_effect) > 0 && strcmp(config.bedtime_effect, "none") != 0) {
        testParams["effect"] = config.bedtime_effect;
      }

      // Appeler handleStartTestBedtime avec les paramètres
      // Le test affichera la couleur, brightness et effet configurés
      if (handleStartTestBedtime(testJson.as<JsonObject>())) {
        Serial.println("[MQTT-ROUTE] set-bedtime-config: Test automatique démarré avec succès");
      } else {
        Serial.println("[MQTT-ROUTE] set-bedtime-config: Erreur lors du démarrage du test automatique");
      }
    }
    
    return true;
  } else {
    Serial.println("[MQTT-ROUTE] set-bedtime-config: Erreur lors de la sauvegarde");
    return false;
  }
}

bool ModelDreamMqttRoutes::handleStartTestWakeup(const JsonObject& json) {
  // Format: { "action": "start-test-wakeup", "params": { "colorR": 255, "colorG": 200, "colorB": 100, "brightness": 50 } }
  // Démarre le test de la couleur et luminosité avec les paramètres fournis
  
  Serial.println("[MQTT-ROUTE] start-test-wakeup: Démarrage/mise à jour du test...");
  
  // Sauvegarder l'état actif pour savoir si c'est une mise à jour ou un nouveau test
  bool wasAlreadyActive = testWakeupActive;
  
  // Récupérer les paramètres
  int colorR = -1;
  int colorG = -1;
  int colorB = -1;
  int brightness = -1;
  
  Serial.print("[MQTT-ROUTE] start-test-wakeup: Message JSON reçu - ");
  serializeJson(json, Serial);
  Serial.println();
  
  // Nouvelle syntaxe avec params
  if (json["params"].is<JsonObject>()) {
    JsonObject params = json["params"].as<JsonObject>();
    Serial.println("[MQTT-ROUTE] start-test-wakeup: Utilisation de la syntaxe avec params");
    if (params["colorR"].is<int>()) colorR = params["colorR"].as<int>();
    if (params["colorG"].is<int>()) colorG = params["colorG"].as<int>();
    if (params["colorB"].is<int>()) colorB = params["colorB"].as<int>();
    if (params["brightness"].is<int>()) brightness = params["brightness"].as<int>();
    
    Serial.print("[MQTT-ROUTE] start-test-wakeup: Paramètres extraits - RGB(");
    Serial.print(colorR);
    Serial.print(",");
    Serial.print(colorG);
    Serial.print(",");
    Serial.print(colorB);
    Serial.print("), Brightness: ");
    Serial.println(brightness);
  }
  // Syntaxe directe (legacy)
  else {
    Serial.println("[MQTT-ROUTE] start-test-wakeup: Utilisation de la syntaxe directe (legacy)");
    if (json["colorR"].is<int>()) colorR = json["colorR"].as<int>();
    if (json["colorG"].is<int>()) colorG = json["colorG"].as<int>();
    if (json["colorB"].is<int>()) colorB = json["colorB"].as<int>();
    if (json["brightness"].is<int>()) brightness = json["brightness"].as<int>();
  }
  
  // Valider les paramètres
  if (colorR < 0 || colorR > 255 || colorG < 0 || colorG > 255 || colorB < 0 || colorB > 255) {
    Serial.println("[MQTT-ROUTE] start-test-wakeup: Couleur invalide");
    return false;
  }
  
  if (brightness < 0 || brightness > 100) {
    Serial.println("[MQTT-ROUTE] start-test-wakeup: Brightness invalide");
    return false;
  }
  
  // Convertir brightness de 0-100 vers 0-255
  uint8_t brightnessValue = LEDManager::brightnessPercentTo255((uint8_t)brightness);
  
  // Sortir du mode sommeil avant d'appliquer le test
  LEDManager::wakeUp();
  
  // Appliquer les paramètres du test
  LEDManager::setEffect(LED_EFFECT_NONE);
  LEDManager::setColor(colorR, colorG, colorB);
  LEDManager::setBrightness(brightnessValue);
  
  // Activer le test (ou le maintenir actif si déjà actif)
  testWakeupActive = true;
  
  // TOUJOURS réinitialiser le timer après avoir validé et appliqué les paramètres
  testWakeupStartTime = millis();
  if (wasAlreadyActive) {
    Serial.println("[MQTT-ROUTE] start-test-wakeup: Test déjà actif, timeout de 15 secondes réinitialisé");
  } else {
    Serial.println("[MQTT-ROUTE] start-test-wakeup: Nouveau test démarré, timeout de 15 secondes initialisé");
  }
  
  Serial.print("[MQTT-ROUTE] start-test-wakeup: Test démarré - Couleur RGB(");
  Serial.print(colorR);
  Serial.print(",");
  Serial.print(colorG);
  Serial.print(",");
  Serial.print(colorB);
  Serial.print("), Brightness: ");
  Serial.print(brightness);
  Serial.println("%");
  
  return true;
}

bool ModelDreamMqttRoutes::handleStopTestWakeup(const JsonObject& json) {
  // Format: { "action": "stop-test-wakeup" }
  // Arrête le test de l'heure de réveil
  
  if (!testWakeupActive) {
    Serial.println("[MQTT-ROUTE] stop-test-wakeup: Aucun test actif");
    return false;
  }
  
  Serial.println("[MQTT-ROUTE] stop-test-wakeup: Arrêt du test");
  
  // Éteindre les LEDs
  LEDManager::clear();
  
  // Désactiver le test
  testWakeupActive = false;
  testWakeupStartTime = 0;
  
  return true;
}

bool ModelDreamMqttRoutes::handleSetWakeupConfig(const JsonObject& json) {
  // Format: { "action": "set-wakeup-config", "params": { "colorR": 255, "colorG": 200, "colorB": 100, "brightness": 50, "autoShutdown": true, "autoShutdownMinutes": 30, "weekdaySchedule": {...} } }
  // Sauvegarde la configuration de l'heure de réveil sur la carte SD

  Serial.println("[MQTT-ROUTE] set-wakeup-config: Sauvegarde de la configuration...");

  if (!SDManager::isAvailable()) {
    Serial.println("[MQTT-ROUTE] set-wakeup-config: Carte SD non disponible");
    return false;
  }

  // Récupérer les paramètres (le serveur envoie à la racine: action, colorR, weekdaySchedule, timestamp — pas de wrapper "params")
  int colorR = -1;
  int colorG = -1;
  int colorB = -1;
  int brightness = -1;
  bool autoShutdown = true;
  int autoShutdownMinutes = 30;
  JsonObject weekdayScheduleObj;
  String weekdayScheduleStr;  // Si reçu comme chaîne JSON
  bool hasWeekdaySchedule = false;
  JsonObject params = json["params"].is<JsonObject>() ? json["params"].as<JsonObject>() : json;

  if (params["colorR"].is<int>()) colorR = params["colorR"].as<int>();
  else if (params["colorR"].is<double>()) colorR = (int)params["colorR"].as<double>();
  if (params["colorG"].is<int>()) colorG = params["colorG"].as<int>();
  else if (params["colorG"].is<double>()) colorG = (int)params["colorG"].as<double>();
  if (params["colorB"].is<int>()) colorB = params["colorB"].as<int>();
  else if (params["colorB"].is<double>()) colorB = (int)params["colorB"].as<double>();
  if (params["brightness"].is<int>()) brightness = params["brightness"].as<int>();
  else if (params["brightness"].is<double>()) brightness = (int)params["brightness"].as<double>();

  if (params["autoShutdown"].is<bool>()) {
    autoShutdown = params["autoShutdown"].as<bool>();
  }

  if (params["autoShutdownMinutes"].is<int>()) {
    autoShutdownMinutes = params["autoShutdownMinutes"].as<int>();
  } else if (params["autoShutdownMinutes"].is<double>()) {
    autoShutdownMinutes = (int)params["autoShutdownMinutes"].as<double>();
  }

  if (params["weekdaySchedule"].is<JsonObject>()) {
    weekdayScheduleObj = params["weekdaySchedule"].as<JsonObject>();
    hasWeekdaySchedule = true;
  } else if (params["weekdaySchedule"].is<const char*>()) {
    weekdayScheduleStr = params["weekdaySchedule"].as<const char*>();
    hasWeekdaySchedule = true;
  }
  
  // Valider les paramètres
  if (colorR < 0 || colorR > 255 || colorG < 0 || colorG > 255 || colorB < 0 || colorB > 255) {
    Serial.println("[MQTT-ROUTE] set-wakeup-config: Couleur invalide");
    return false;
  }
  
  if (brightness < 0 || brightness > 100) {
    Serial.println("[MQTT-ROUTE] set-wakeup-config: Brightness invalide");
    return false;
  }
  
  // Charger la configuration actuelle depuis la SD
  SDConfig config = SDManager::getConfig();
  
  // Valider autoShutdownMinutes
  if (autoShutdownMinutes < 5) {
    autoShutdownMinutes = 5;
  }
  if (autoShutdownMinutes > 120) {
    autoShutdownMinutes = 120;
  }

  // Mettre à jour les champs wakeup
  config.wakeup_colorR = (uint8_t)colorR;
  config.wakeup_colorG = (uint8_t)colorG;
  config.wakeup_colorB = (uint8_t)colorB;
  config.wakeup_brightness = (uint8_t)brightness;
  config.wakeup_autoShutdown = autoShutdown;
  config.wakeup_autoShutdownMinutes = (uint16_t)autoShutdownMinutes;
  
  // Sauvegarder weekdaySchedule si présent
  if (hasWeekdaySchedule) {
    String scheduleStr = weekdayScheduleStr.length() > 0 ? weekdayScheduleStr : String();
    if (scheduleStr.length() == 0) {
      serializeJson(weekdayScheduleObj, scheduleStr);
    }
    if (scheduleStr.length() < sizeof(config.wakeup_weekdaySchedule)) {
      strncpy(config.wakeup_weekdaySchedule, scheduleStr.c_str(), sizeof(config.wakeup_weekdaySchedule) - 1);
      config.wakeup_weekdaySchedule[sizeof(config.wakeup_weekdaySchedule) - 1] = '\0';
      Serial.print("[MQTT-ROUTE] set-wakeup-config: weekdaySchedule sauvegardé: ");
      Serial.println(config.wakeup_weekdaySchedule);
    } else {
      Serial.println("[MQTT-ROUTE] set-wakeup-config: weekdaySchedule trop grand, tronqué");
      strncpy(config.wakeup_weekdaySchedule, scheduleStr.c_str(), sizeof(config.wakeup_weekdaySchedule) - 1);
      config.wakeup_weekdaySchedule[sizeof(config.wakeup_weekdaySchedule) - 1] = '\0';
    }
  } else {
    // Pas de weekdaySchedule reçu, garder la valeur existante ou mettre un objet vide
    if (strlen(config.wakeup_weekdaySchedule) == 0) {
      strcpy(config.wakeup_weekdaySchedule, "{}");
    }
  }
  
  // Sauvegarder sur la SD
  if (SDManager::saveConfig(config)) {
    Serial.print("[MQTT-ROUTE] set-wakeup-config: Configuration sauvegardée - RGB(");
    Serial.print(colorR);
    Serial.print(",");
    Serial.print(colorG);
    Serial.print(",");
    Serial.print(colorB);
    Serial.print("), Brightness: ");
    Serial.print(brightness);
    Serial.print("%, AutoShutdown: ");
    Serial.print(autoShutdown ? "true" : "false");
    Serial.print(", Duration: ");
    Serial.print(autoShutdownMinutes);
    Serial.print("min");
    if (hasWeekdaySchedule) {
      Serial.print(", weekdaySchedule: ");
      Serial.println(config.wakeup_weekdaySchedule);
    } else {
      Serial.println();
    }
    
    // Recharger la configuration dans le WakeupManager
    WakeupManager::reloadConfig();

    // Si on est en mode bedtime, reprendre la config bedtime pour l'afficher (ne pas lancer le test wakeup)
    if (BedtimeManager::isBedtimeActive()) {
      Serial.println("[MQTT-ROUTE] set-wakeup-config: Mode bedtime actif, réapplication de la config bedtime");
      BedtimeManager::restoreDisplayFromConfig();
    } else if (WakeupManager::isWakeupActive()) {
      // Wakeup actif, ne pas lancer le test pour ne pas interrompre la routine
      Serial.println("[MQTT-ROUTE] set-wakeup-config: Mode wakeup actif, test non lancé pour ne pas interrompre");
    } else {
      // Déclencher automatiquement le test avec la nouvelle configuration
      Serial.println("[MQTT-ROUTE] set-wakeup-config: Déclenchement automatique du test...");

      JsonDocument testJson;
      JsonObject testParams = testJson["params"].to<JsonObject>();
      testParams["colorR"] = config.wakeup_colorR;
      testParams["colorG"] = config.wakeup_colorG;
      testParams["colorB"] = config.wakeup_colorB;
      testParams["brightness"] = config.wakeup_brightness;

      if (handleStartTestWakeup(testJson.as<JsonObject>())) {
        Serial.println("[MQTT-ROUTE] set-wakeup-config: Test automatique démarré avec succès");
      } else {
        Serial.println("[MQTT-ROUTE] set-wakeup-config: Erreur lors du démarrage du test automatique");
      }
    }
    
    return true;
  } else {
    Serial.println("[MQTT-ROUTE] set-wakeup-config: Erreur lors de la sauvegarde");
    return false;
  }
}

bool ModelDreamMqttRoutes::handleFirmwareUpdate(const JsonObject& json) {
  // Format: { "action": "firmware-update", "params": { "version": "1.0.1" } }
  const char* version = nullptr;
  if (json["params"].is<JsonObject>()) {
    JsonObject params = json["params"].as<JsonObject>();
    if (params["version"].is<const char*>()) version = params["version"].as<const char*>();
  }
  if (json["version"].is<const char*>()) version = json["version"].as<const char*>();
  if (version == nullptr || strlen(version) == 0) {
    Serial.println("[MQTT-ROUTE] firmware-update: version manquante");
    return false;
  }

  Serial.print("[MQTT-ROUTE] firmware-update: version cible ");
  Serial.println(version);

#ifdef HAS_WIFI
  return OTAManager::startUpdateTask(version);
#else
  Serial.println("[MQTT-ROUTE] firmware-update: WiFi non disponible sur ce build");
  return false;
#endif
}

bool ModelDreamMqttRoutes::handleGetEnv(const JsonObject& json) {
  (void)json;
  Serial.println("[MQTT-ROUTE] get-env: Lecture temperature, humidite, pression...");

#ifdef HAS_ENV_SENSOR
  if (!EnvSensorManager::isInitialized() || !EnvSensorManager::isAvailable()) {
    const char* errJson = "{\"type\":\"env\",\"available\":false,\"error\":\"sensor not available\"}";
    if (MqttManager::publish(errJson)) {
      Serial.println("[MQTT-ROUTE] get-env: Capteur non disponible, reponse publiee");
    }
    return true;
  }

  float t = EnvSensorManager::getTemperatureC();
  float h = EnvSensorManager::getHumidityPercent();
  float p = EnvSensorManager::getPressurePa();

  // Format JSON garanti (évite locale/notation scientifique qui peut invalider le JSON)
  char tStr[16], hStr[16], pStr[16];
  if (!isfinite(t) || isnan(t) || t < -50.0f || t > 150.0f) {
    strcpy(tStr, "null");
  } else {
    int ti = (int)t;
    int td = (int)((t - (float)ti) * 10);
    if (td < 0) td = -td;
    snprintf(tStr, sizeof(tStr), "%d.%d", ti, td);
  }
  if (!isfinite(h) || isnan(h) || h < 0.0f || h > 100.0f) {
    strcpy(hStr, "null");
  } else {
    int hi = (int)h;
    int hd = (int)((h - (float)hi) * 10);
    if (hd < 0) hd = -hd;
    snprintf(hStr, sizeof(hStr), "%d.%d", hi, hd);
  }
  if (!isfinite(p) || isnan(p) || p < 10000.0f || p > 120000.0f) {
    strcpy(pStr, "null");
  } else {
    snprintf(pStr, sizeof(pStr), "%d", (int)p);
  }

  char envJson[512];
  snprintf(envJson, sizeof(envJson),
    "{\"type\":\"env\",\"available\":true,\"temperatureC\":%s,\"humidityPercent\":%s,\"pressurePa\":%s}",
    tStr, hStr, pStr);

  if (MqttManager::publish(envJson)) {
    Serial.println("[MQTT-ROUTE] get-env: Donnees env publiees (temp, humidite, pression)");
  } else {
    Serial.println("[MQTT-ROUTE] get-env: Erreur publication");
  }
  return true;
#else
  const char* errJson = "{\"type\":\"env\",\"available\":false,\"error\":\"env sensor not enabled\"}";
  if (MqttManager::publish(errJson)) {
    Serial.println("[MQTT-ROUTE] get-env: Capteur non active sur ce firmware");
  }
  return true;
#endif
}

bool ModelDreamMqttRoutes::handleSetNighttimeAlert(const JsonObject& json) {
  // Format: { "action": "set-nighttime-alert", "params": { "enabled": true } }
  // Ou: { "action": "set-nighttime-alert", "enabled": true }
  // Sauvegarde la config Dream (clé "dream") via DreamConfigManager

  bool enabled = false;

  if (json["params"].is<JsonObject>()) {
    JsonObject params = json["params"].as<JsonObject>();
    if (params["enabled"].is<bool>()) {
      enabled = params["enabled"].as<bool>();
    }
  } else if (json["enabled"].is<bool>()) {
    enabled = json["enabled"].as<bool>();
  }

  if (!SDManager::isAvailable()) {
    Serial.println("[MQTT-ROUTE] set-nighttime-alert: Carte SD non disponible");
    return false;
  }

  DreamConfig config = DreamConfigManager::getConfig();
  config.nighttime_alert_enabled = enabled;
  if (DreamConfigManager::saveConfig(config)) {
    Serial.print("[MQTT-ROUTE] Alerte reveil nocturne: ");
    Serial.println(enabled ? "activee" : "desactivee");
    return true;
  }
  Serial.println("[MQTT-ROUTE] set-nighttime-alert: Erreur sauvegarde");
  return false;
}

// Seuils de changement pour publication proactive (évite bruit)
#define ENV_TEMP_THRESHOLD_C    0.5f  // Envoyer seulement si température change d'au moins 0.5°C
#define ENV_HUMIDITY_THRESHOLD 1.0f
#define ENV_PUBLISH_INTERVAL_MS 30000  // Au plus toutes les 30 s

void ModelDreamMqttRoutes::updateEnvPublisher() {
#ifdef HAS_ENV_SENSOR
  if (!EnvSensorManager::isInitialized() || !EnvSensorManager::isAvailable()) return;
  if (!MqttManager::isConnected()) return;

  static unsigned long lastPublishMs = 0;
  static float lastTempC = NAN;
  static float lastHumPercent = NAN;
  static bool firstRun = true;

  unsigned long now = millis();
  if (!firstRun && (now - lastPublishMs) < ENV_PUBLISH_INTERVAL_MS) return;

  float t = EnvSensorManager::getTemperatureC();
  float h = EnvSensorManager::getHumidityPercent();
  float p = EnvSensorManager::getPressurePa();

  bool changed = firstRun;
  if (!firstRun) {
    if (isfinite(t) && isfinite(lastTempC) && fabsf(t - lastTempC) >= ENV_TEMP_THRESHOLD_C) changed = true;
    if (isfinite(h) && isfinite(lastHumPercent) && fabsf(h - lastHumPercent) >= ENV_HUMIDITY_THRESHOLD) changed = true;
    if (isfinite(t) != isfinite(lastTempC) || isfinite(h) != isfinite(lastHumPercent)) changed = true;
  }

  if (!changed) return;

  firstRun = false;
  lastPublishMs = now;
  lastTempC = t;
  lastHumPercent = h;

  char tStr[16], hStr[16], pStr[16];
  if (!isfinite(t) || isnan(t)) strcpy(tStr, "null");
  else { int ti = (int)t; int td = (int)((t - (float)ti) * 10); if (td < 0) td = -td; snprintf(tStr, sizeof(tStr), "%d.%d", ti, td); }
  if (!isfinite(h) || isnan(h)) strcpy(hStr, "null");
  else { int hi = (int)h; int hd = (int)((h - (float)hi) * 10); if (hd < 0) hd = -hd; snprintf(hStr, sizeof(hStr), "%d.%d", hi, hd); }
  if (!isfinite(p) || isnan(p) || p < 10000.0f || p > 120000.0f) strcpy(pStr, "null");
  else snprintf(pStr, sizeof(pStr), "%d", (int)p);

  char envJson[512];
  snprintf(envJson, sizeof(envJson),
    "{\"type\":\"env\",\"available\":true,\"temperatureC\":%s,\"humidityPercent\":%s,\"pressurePa\":%s}",
    tStr, hStr, pStr);

  if (MqttManager::publish(envJson)) {
    // Publication proactive env - pas de log pour réduire le bruit
  }
#endif
}

void ModelDreamMqttRoutes::publishRoutineState(const char* routine, const char* state) {
  // Utiliser isInitialized() plutôt que isConnected() car MqttManager a une queue interne
  // Cela permet de publier les messages même pendant la connexion initiale
  if (!MqttManager::isInitialized()) return;
  char json[128];
  snprintf(json, sizeof(json), "{\"type\":\"routine\",\"routine\":\"%s\",\"state\":\"%s\"}", routine, state);
  if (MqttManager::publish(json)) {
    Serial.printf("[MQTT-ROUTE] routine: %s %s publie\n", routine, state);
  } else {
    Serial.printf("[MQTT-ROUTE] routine: %s %s ECHEC publication\n", routine, state);
  }
}

void ModelDreamMqttRoutes::publishNighttimeAlertToggled(bool enabled) {
  // Utiliser isInitialized() plutôt que isConnected() pour permettre la mise en queue
  if (!MqttManager::isInitialized()) return;
  char json[64];
  snprintf(json, sizeof(json), "{\"type\":\"nighttime-alert-toggled\",\"enabled\":%s}", enabled ? "true" : "false");
  if (MqttManager::publish(json)) {
    Serial.printf("[MQTT-ROUTE] nighttime-alert-toggled: %s\n", enabled ? "on" : "off");
  }
}

void ModelDreamMqttRoutes::publishDefaultColorState() {
  // Utiliser isInitialized() plutôt que isConnected() pour permettre la mise en queue
  if (!MqttManager::isInitialized()) return;

  // Déterminer le deviceState basé sur l'état courant
  const char* deviceState = "idle";
  if (BedtimeManager::isBedtimeActive()) {
    deviceState = BedtimeManager::isManuallyStarted() ? "manual" : "bedtime";
  } else if (WakeupManager::isWakeupActive()) {
    deviceState = "wakeup";
  } else if (DreamTouchHandler::isDefaultColorDisplayed()) {
    deviceState = "manual";
  }

  // Publier un message "info" simplifié avec le deviceState
  char json[128];
  snprintf(json, sizeof(json), "{\"type\":\"info\",\"deviceState\":\"%s\"}", deviceState);
  if (MqttManager::publish(json)) {
    Serial.printf("[MQTT-ROUTE] default-color-state publie (deviceState=%s)\n", deviceState);
  }
}

bool ModelDreamMqttRoutes::handleSetTimezone(const JsonObject& json) {
  // Format: { "action": "set-timezone", "timezoneId": "Europe/Paris" }

  if (!json["timezoneId"].is<const char*>()) {
    Serial.println("[MQTT-ROUTE] set-timezone: timezoneId manquant ou invalide");
    return false;
  }

  const char* timezoneId = json["timezoneId"].as<const char*>();
  if (!timezoneId || strlen(timezoneId) == 0) {
    Serial.println("[MQTT-ROUTE] set-timezone: timezoneId vide");
    return false;
  }

  Serial.printf("[MQTT-ROUTE] set-timezone: Réception de %s\n", timezoneId);

  // Sauvegarder timezoneId dans config.json pour getLocalDateTime()
  if (SDManager::isAvailable() && SDManager::configFileExists()) {
    File configFile = SD.open("/config.json", FILE_READ);
    if (configFile) {
      const size_t maxSize = 4096;
      char jsonBuffer[4096];
      size_t fileSize = configFile.size();
      if (fileSize > 0 && fileSize < maxSize) {
        size_t bytesRead = configFile.readBytes(jsonBuffer, maxSize - 1);
        jsonBuffer[bytesRead] = '\0';
        configFile.close();

        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        StaticJsonDocument<4096> doc;
        #pragma GCC diagnostic pop
        if (!deserializeJson(doc, jsonBuffer)) {
          doc["timezoneId"] = timezoneId;
          configFile = SD.open("/config.json", FILE_WRITE);
          if (configFile) {
            serializeJson(doc, configFile);
            configFile.close();
            RTCManager::setTimezoneId(timezoneId);
            Serial.printf("[MQTT-ROUTE] set-timezone: timezoneId sauvegardé dans config.json\n");
          }
        } else {
          configFile.close();
        }
      } else {
        configFile.close();
      }
    }
  }

  // RTC stocke toujours UTC. syncWithNTP(0,0) pour garantir cohérence avec getLocalDateTime().
  // Le fuseau est appliqué à la lecture via getLocalDateTime() (lit timezoneId dans config.json).
  if (RTCManager::syncWithNTP(0, 0)) {
    Serial.printf("[MQTT-ROUTE] set-timezone: RTC synchronisé avec %s\n", timezoneId);
    return true;
  } else {
    Serial.println("[MQTT-ROUTE] set-timezone: Erreur lors de la synchronisation du RTC");
    return false;
  }
}

void ModelDreamMqttRoutes::printRoutes() {
  Serial.println("");
  Serial.println("========== Routes MQTT Dream ==========");
  Serial.println("{ \"action\": \"get-info\" }");
  Serial.println("{ \"action\": \"brightness\", \"params\": { \"value\": 1-100 } }");
  Serial.println("{ \"action\": \"sleep-timeout\", \"params\": { \"value\": 0|5000-300000 } }");
  Serial.println("{ \"action\": \"reboot\", \"params\": { \"delay\": ms } }");
  Serial.println("{ \"action\": \"led\", \"color\": \"#RRGGBB\" }");
  Serial.println("{ \"action\": \"led\", \"effect\": \"none|pulse|rotate|rainbow|glossy|off\" }");
  Serial.println("{ \"action\": \"start-test-bedtime\", \"params\": { \"colorR\": 0-255, \"colorG\": 0-255, \"colorB\": 0-255, \"brightness\": 0-100 } }");
  Serial.println("{ \"action\": \"stop-test-bedtime\" }");
  Serial.println("{ \"action\": \"start-bedtime\" }");
  Serial.println("{ \"action\": \"stop-bedtime\" }");
  Serial.println("{ \"action\": \"stop-routine\" }");
  Serial.println("{ \"action\": \"set-bedtime-config\", \"params\": { \"colorR\": 0-255, \"colorG\": 0-255, \"colorB\": 0-255, \"brightness\": 0-100, \"allNight\": true|false, \"weekdaySchedule\": {...} } }");
  Serial.println("{ \"action\": \"start-test-wakeup\", \"params\": { \"colorR\": 0-255, \"colorG\": 0-255, \"colorB\": 0-255, \"brightness\": 0-100 } }");
  Serial.println("{ \"action\": \"stop-test-wakeup\" }");
  Serial.println("{ \"action\": \"set-wakeup-config\", \"params\": { \"colorR\": 0-255, \"colorG\": 0-255, \"colorB\": 0-255, \"brightness\": 0-100, \"weekdaySchedule\": {...} } }");
  Serial.println("{ \"action\": \"firmware-update\", \"version\": \"1.0.1\" }");
  Serial.println("{ \"action\": \"get-env\" }");
  Serial.println("{ \"action\": \"set-nighttime-alert\", \"params\": { \"enabled\": true|false } }");
  Serial.println("{ \"action\": \"nighttime-alert-ack\" }  // J'arrive - rainbow 5 sec");
  Serial.println("{ \"action\": \"set-default-config\", \"colorR\": 0-255, \"colorG\": 0-255, \"colorB\": 0-255, \"brightness\": 0-100, \"effect\": \"string|null\" }");
  Serial.println("{ \"action\": \"start-test-default-config\", \"params\": { \"colorR\": 0-255, \"colorG\": 0-255, \"colorB\": 0-255, \"brightness\": 0-100, \"effect\": \"string|null\" } }");
  Serial.println("{ \"action\": \"stop-test-default-config\" }");
  Serial.println("==========================================");
}
