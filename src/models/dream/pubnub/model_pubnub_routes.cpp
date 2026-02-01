#include "model_pubnub_routes.h"
#include "../../common/managers/led/led_manager.h"
#include "../../common/managers/init/init_manager.h"
#include "../../common/managers/wifi/wifi_manager.h"
#include "../../common/managers/pubnub/pubnub_manager.h"
#include "../../common/managers/sd/sd_manager.h"
#include "../../common/managers/nfc/nfc_manager.h"
#include "../../common/utils/mac_utils.h"
#include "../managers/bedtime/bedtime_manager.h"
#include "../managers/wakeup/wakeup_manager.h"
#include <limits.h>   // Pour ULONG_MAX

/**
 * Routes PubNub spécifiques au modèle Kidoo Dream
 */

bool ModelDreamPubNubRoutes::processMessage(const JsonObject& json) {
  // Vérifier que l'action est présente
  if (!json["action"].is<const char*>()) {
    Serial.println("[PUBNUB-ROUTE] Erreur: action manquante dans le message");
    return false;
  }
  
  const char* action = json["action"].as<const char*>();
  if (action == nullptr) {
    Serial.println("[PUBNUB-ROUTE] Erreur: action est null");
    return false;
  }
  
  Serial.print("[PUBNUB-ROUTE] Traitement de l'action: ");
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
  
  Serial.print("[PUBNUB-ROUTE] Action inconnue: ");
  Serial.println(action);
  return false;
}

bool ModelDreamPubNubRoutes::handleGetInfo(const JsonObject& json) {
  // Format: { "action": "get-info" }
  // Publie les informations complètes de l'appareil
  
  Serial.println("[PUBNUB-ROUTE] get-info: Préparation des informations du Kidoo...");
  
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
  
  // Récupérer l'adresse MAC WiFi (utilisée pour PubNub)
  // Sur ESP32-C3, BLE et WiFi ont des adresses MAC différentes
  char macStr[18];
  if (!getMacAddressString(macStr, sizeof(macStr), ESP_MAC_WIFI_STA)) {
    strcpy(macStr, "00:00:00:00:00:00"); // Valeur par défaut en cas d'erreur
  }
  
  // Construire le JSON de réponse
  // Note: On utilise un buffer assez grand pour toutes les infos
  char infoJson[512];
  snprintf(infoJson, sizeof(infoJson),
    "{"
      "\"type\":\"info\","
      "\"device\":\"%s\","
      "\"mac\":\"%s\","
      "\"ip\":\"%s\","
      "\"model\":\"dream\","
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
      "}"
    "}",
    DEFAULT_DEVICE_NAME,
    macStr,
    WiFiManager::getLocalIP().c_str(),
    millis() / 1000,  // uptime en secondes
    ESP.getFreeHeap(),
    config.wifi_ssid,
    WiFiManager::getRSSI(),
    (config.led_brightness * 100 + 127) / 255,  // brightness en % avec arrondi correct
    config.sleep_timeout_ms,
    totalBytes,
    freeBytes,
    usedBytes,
    NFCManager::isAvailable() ? "true" : "false"
  );
  
  if (PubNubManager::publish(infoJson)) {
    Serial.println("[PUBNUB-ROUTE] get-info: Informations publiees avec succes");
  } else {
    Serial.println("[PUBNUB-ROUTE] get-info: Erreur lors de la publication des informations");
  }
  
  return true;
}

bool ModelDreamPubNubRoutes::handleBrightness(const JsonObject& json) {
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
    Serial.println("[PUBNUB-ROUTE] brightness: parametre 'value' manquant");
    return false;
  }
  
  // Valider la plage (0-100%)
  if (value < 0) value = 0;
  if (value > 100) value = 100;
  
  // Convertir en 0-255 avec arrondi correct
  // Formule: (value * 255 + 50) / 100 pour arrondir correctement
  // 0% → 0, 50% → 127, 100% → 255
  uint8_t brightness = (value * 255 + 50) / 100;
  
  if (LEDManager::setBrightness(brightness)) {
    Serial.print("[PUBNUB-ROUTE] Luminosite: ");
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

bool ModelDreamPubNubRoutes::handleSleepTimeout(const JsonObject& json) {
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
    Serial.println("[PUBNUB-ROUTE] sleep-timeout: parametre 'value' ou 'timeout' manquant");
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
    Serial.println("[PUBNUB-ROUTE] Sleep mode desactive");
  } else {
    Serial.print("[PUBNUB-ROUTE] Sleep timeout: ");
    Serial.print(timeout);
    Serial.println(" ms");
  }
  
  return true;
}

bool ModelDreamPubNubRoutes::handleReboot(const JsonObject& json) {
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
    Serial.print("[PUBNUB-ROUTE] Redemarrage dans ");
    Serial.print(delayMs);
    Serial.println(" ms");
    delay(delayMs);
  } else {
    Serial.println("[PUBNUB-ROUTE] Redemarrage immediat");
  }
  
  ESP.restart();
  
  return true;  // Ne sera jamais atteint
}

bool ModelDreamPubNubRoutes::handleLed(const JsonObject& json) {
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
    Serial.print("[PUBNUB-ROUTE] Couleur: ");
    Serial.println(colorStr);
    handled = true;
  }
  
  // Traiter l'effet
  if (json["effect"].is<const char*>()) {
    const char* effectStr = json["effect"].as<const char*>();
    LEDEffect effect = LED_EFFECT_NONE;
    
    if (strcmp(effectStr, "none") == 0 || strcmp(effectStr, "solid") == 0) effect = LED_EFFECT_NONE;
    else if (strcmp(effectStr, "pulse") == 0) effect = LED_EFFECT_PULSE;
    else if (strcmp(effectStr, "rotate") == 0) effect = LED_EFFECT_ROTATE;
    else if (strcmp(effectStr, "rainbow") == 0) effect = LED_EFFECT_RAINBOW;
    else if (strcmp(effectStr, "glossy") == 0) effect = LED_EFFECT_GLOSSY;
    else if (strcmp(effectStr, "off") == 0) {
      // Éteindre les LEDs
      LEDManager::clear();
      Serial.println("[PUBNUB-ROUTE] LEDs eteintes");
      return true;
    }
    
    LEDManager::setEffect(effect);
    Serial.print("[PUBNUB-ROUTE] Effet: ");
    Serial.println(effectStr);
    handled = true;
  }
  
  if (!handled) {
    Serial.println("[PUBNUB-ROUTE] led: parametre 'color' ou 'effect' manquant");
  }
  
  return handled;
}

// Variables statiques pour gérer l'état du test de bedtime
static bool testBedtimeActive = false;
static unsigned long testBedtimeStartTime = 0;
static bool testWakeupActive = false;
static unsigned long testWakeupStartTime = 0;
static const unsigned long TEST_BEDTIME_TIMEOUT_MS = 15000; // 15 secondes

bool ModelDreamPubNubRoutes::handleStartTestBedtime(const JsonObject& json) {
  // Format: { "action": "start-test-bedtime", "params": { "colorR": 255, "colorG": 107, "colorB": 107, "brightness": 50 } }
  // Démarre le test de la couleur et luminosité avec les paramètres fournis
  
  Serial.println("[PUBNUB-ROUTE] start-test-bedtime: Démarrage/mise à jour du test...");
  
  // Sauvegarder l'état actif pour savoir si c'est une mise à jour ou un nouveau test
  bool wasAlreadyActive = testBedtimeActive;
  
  // Récupérer les paramètres
  int colorR = -1;
  int colorG = -1;
  int colorB = -1;
  int brightness = -1;
  const char* effectStr = nullptr;
  bool hasEffect = false;
  
  Serial.print("[PUBNUB-ROUTE] start-test-bedtime: Message JSON reçu - ");
  serializeJson(json, Serial);
  Serial.println();
  
  // Nouvelle syntaxe avec params
  if (json["params"].is<JsonObject>()) {
    JsonObject params = json["params"].as<JsonObject>();
    Serial.println("[PUBNUB-ROUTE] start-test-bedtime: Utilisation de la syntaxe avec params");
    if (params["colorR"].is<int>()) colorR = params["colorR"].as<int>();
    if (params["colorG"].is<int>()) colorG = params["colorG"].as<int>();
    if (params["colorB"].is<int>()) colorB = params["colorB"].as<int>();
    if (params["brightness"].is<int>()) brightness = params["brightness"].as<int>();
    if (params["effect"].is<const char*>()) {
      effectStr = params["effect"].as<const char*>();
      hasEffect = true;
    }
    
    Serial.print("[PUBNUB-ROUTE] start-test-bedtime: Paramètres extraits - RGB(");
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
    Serial.println("[PUBNUB-ROUTE] start-test-bedtime: Utilisation de la syntaxe directe (legacy)");
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
    Serial.println("[PUBNUB-ROUTE] start-test-bedtime: Couleur invalide");
    return false;
  }
  
  if (brightness < 0 || brightness > 100) {
    Serial.println("[PUBNUB-ROUTE] start-test-bedtime: Brightness invalide");
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
    if (strcmp(effectStr, "none") == 0 || strcmp(effectStr, "") == 0) {
      effect = LED_EFFECT_NONE;
    } else if (strcmp(effectStr, "pulse") == 0) {
      effect = LED_EFFECT_PULSE;
    } else if (strcmp(effectStr, "rotate") == 0) {
      effect = LED_EFFECT_ROTATE;
    } else if (strcmp(effectStr, "rainbow") == 0) {
      effect = LED_EFFECT_RAINBOW;
    } else if (strcmp(effectStr, "rainbow-soft") == 0) {
      effect = LED_EFFECT_RAINBOW_SOFT;
    } else if (strcmp(effectStr, "glossy") == 0) {
      effect = LED_EFFECT_GLOSSY;
    } else if (strcmp(effectStr, "breathe") == 0) {
      effect = LED_EFFECT_BREATHE;
    } else if (strcmp(effectStr, "nightlight") == 0) {
      effect = LED_EFFECT_NIGHTLIGHT;
    } else {
      Serial.printf("[PUBNUB-ROUTE] start-test-bedtime: Effet inconnu '%s', utilisation de NONE\n", effectStr);
      effect = LED_EFFECT_NONE;
    }
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
    Serial.println("[PUBNUB-ROUTE] start-test-bedtime: Test déjà actif, timeout de 15 secondes réinitialisé");
  } else {
    Serial.println("[PUBNUB-ROUTE] start-test-bedtime: Nouveau test démarré, timeout de 15 secondes initialisé");
  }
  
  Serial.print("[PUBNUB-ROUTE] start-test-bedtime: Test démarré - Couleur RGB(");
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

bool ModelDreamPubNubRoutes::handleStartBedtime(const JsonObject& json) {
  // Format: { "action": "start-bedtime" }
  // Démarre manuellement la routine de coucher avec la configuration sauvegardée
  // Empêche le déclenchement automatique programmé
  
  Serial.println("[PUBNUB-ROUTE] start-bedtime: Démarrage manuel de la routine de coucher");
  
  // Vérifier que BedtimeManager est initialisé
  if (!BedtimeManager::isBedtimeEnabled()) {
    Serial.println("[PUBNUB-ROUTE] start-bedtime: ERREUR - Bedtime non configuré ou non activé");
    return false;
  }
  
  // Vérifier si le bedtime est déjà actif
  if (BedtimeManager::isBedtimeActive()) {
    Serial.println("[PUBNUB-ROUTE] start-bedtime: Bedtime déjà actif");
    return true; // Pas une erreur, juste déjà actif
  }
  
  // Démarrer le bedtime manuellement
  BedtimeManager::startBedtimeManually();
  
  Serial.println("[PUBNUB-ROUTE] start-bedtime: Routine de coucher démarrée manuellement");
  return true;
}

bool ModelDreamPubNubRoutes::handleStopBedtime(const JsonObject& json) {
  // Format: { "action": "stop-bedtime" }
  // Arrête manuellement la routine de coucher
  
  Serial.println("[PUBNUB-ROUTE] stop-bedtime: Arrêt manuel de la routine de coucher");
  
  // Vérifier si le bedtime est actif
  if (!BedtimeManager::isBedtimeActive()) {
    Serial.println("[PUBNUB-ROUTE] stop-bedtime: Aucun bedtime actif");
    return false;
  }
  
  // Arrêter le bedtime manuellement
  BedtimeManager::stopBedtimeManually();
  
  Serial.println("[PUBNUB-ROUTE] stop-bedtime: Routine de coucher arrêtée manuellement");
  return true;
}

bool ModelDreamPubNubRoutes::handleStopRoutine(const JsonObject& json) {
  // Format: { "action": "stop-routine" }
  // Arrête la routine active (bedtime ou wakeup)
  
  Serial.println("[PUBNUB-ROUTE] stop-routine: Arrêt de la routine active");
  
  bool stopped = false;
  
  // Vérifier si le bedtime est actif
  if (BedtimeManager::isBedtimeActive()) {
    Serial.println("[PUBNUB-ROUTE] stop-routine: Arrêt du bedtime actif");
    BedtimeManager::stopBedtimeManually();
    stopped = true;
  }
  
  // Vérifier si le wakeup est actif
  if (WakeupManager::isWakeupActive()) {
    Serial.println("[PUBNUB-ROUTE] stop-routine: Arrêt du wakeup actif");
    WakeupManager::stopWakeupManually();
    stopped = true;
  }
  
  if (!stopped) {
    Serial.println("[PUBNUB-ROUTE] stop-routine: Aucune routine active");
    return false;
  }
  
  Serial.println("[PUBNUB-ROUTE] stop-routine: Routine arrêtée");
  return true;
}

bool ModelDreamPubNubRoutes::handleStopTestBedtime(const JsonObject& json) {
  // Format: { "action": "stop-test-bedtime" }
  // Arrête le test de l'heure de coucher
  
  if (!testBedtimeActive) {
    Serial.println("[PUBNUB-ROUTE] stop-test-bedtime: Aucun test actif");
    return false;
  }
  
  Serial.println("[PUBNUB-ROUTE] stop-test-bedtime: Arrêt du test");
  
  // Éteindre les LEDs
  LEDManager::clear();
  
  // Désactiver le test
  testBedtimeActive = false;
  testBedtimeStartTime = 0;
  
  return true;
}

void ModelDreamPubNubRoutes::checkTestBedtimeTimeout() {
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
      Serial.println("[PUBNUB-ROUTE] Test bedtime: Timeout de 15 secondes atteint, arrêt automatique");
      
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

void ModelDreamPubNubRoutes::checkTestWakeupTimeout() {
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
      Serial.println("[PUBNUB-ROUTE] checkTestWakeupTimeout: Timeout de 15 secondes dépassé, arrêt du test");
      
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

bool ModelDreamPubNubRoutes::isTestWakeupActive() {
  return testWakeupActive;
}

bool ModelDreamPubNubRoutes::isTestBedtimeActive() {
  return testBedtimeActive;
}

bool ModelDreamPubNubRoutes::handleSetBedtimeConfig(const JsonObject& json) {
  // Format: { "action": "set-bedtime-config", "params": { "colorR": 255, "colorG": 107, "colorB": 107, "brightness": 50, "allNight": false, "weekdaySchedule": {...} } }
  // Sauvegarde la configuration de l'heure de coucher sur la carte SD
  
  Serial.println("[PUBNUB-ROUTE] set-bedtime-config: Sauvegarde de la configuration...");
  
  if (!SDManager::isAvailable()) {
    Serial.println("[PUBNUB-ROUTE] set-bedtime-config: Carte SD non disponible");
    return false;
  }
  
  // Récupérer les paramètres
  int colorR = -1;
  int colorG = -1;
  int colorB = -1;
  int brightness = -1;
  bool allNight = false;
  const char* effectStr = nullptr;
  bool hasEffect = false;
  JsonObject weekdayScheduleObj;
  bool hasWeekdaySchedule = false;
  
  // Nouvelle syntaxe avec params
  if (json["params"].is<JsonObject>()) {
    JsonObject params = json["params"].as<JsonObject>();
    Serial.println("[PUBNUB-ROUTE] set-bedtime-config: Utilisation de la syntaxe avec params");
    
    if (params["colorR"].is<int>()) colorR = params["colorR"].as<int>();
    if (params["colorG"].is<int>()) colorG = params["colorG"].as<int>();
    if (params["colorB"].is<int>()) colorB = params["colorB"].as<int>();
    if (params["brightness"].is<int>()) brightness = params["brightness"].as<int>();
    if (params["allNight"].is<bool>()) allNight = params["allNight"].as<bool>();
    
    // Récupérer l'effet si présent
    if (params["effect"].is<const char*>()) {
      effectStr = params["effect"].as<const char*>();
      hasEffect = true;
    }
    
    // Récupérer weekdaySchedule si présent
    if (params["weekdaySchedule"].is<JsonObject>()) {
      weekdayScheduleObj = params["weekdaySchedule"].as<JsonObject>();
      hasWeekdaySchedule = true;
    }
  }
  // Syntaxe directe (legacy)
  else {
    Serial.println("[PUBNUB-ROUTE] set-bedtime-config: Utilisation de la syntaxe directe (legacy)");
    if (json["colorR"].is<int>()) colorR = json["colorR"].as<int>();
    if (json["colorG"].is<int>()) colorG = json["colorG"].as<int>();
    if (json["colorB"].is<int>()) colorB = json["colorB"].as<int>();
    if (json["brightness"].is<int>()) brightness = json["brightness"].as<int>();
    if (json["allNight"].is<bool>()) allNight = json["allNight"].as<bool>();
    
    // Récupérer l'effet si présent
    if (json["effect"].is<const char*>()) {
      effectStr = json["effect"].as<const char*>();
      hasEffect = true;
    }
    
    // Récupérer weekdaySchedule si présent
    if (json["weekdaySchedule"].is<JsonObject>()) {
      weekdayScheduleObj = json["weekdaySchedule"].as<JsonObject>();
      hasWeekdaySchedule = true;
    }
  }
  
  // Valider les paramètres
  // Si un effet est fourni, la couleur n'est pas obligatoire (mais peut être fournie pour l'effet ROTATE)
  if (!hasEffect && (colorR < 0 || colorR > 255 || colorG < 0 || colorG > 255 || colorB < 0 || colorB > 255)) {
    Serial.println("[PUBNUB-ROUTE] set-bedtime-config: Couleur invalide (requise si pas d'effet)");
    return false;
  }
  
  if (brightness < 0 || brightness > 100) {
    Serial.println("[PUBNUB-ROUTE] set-bedtime-config: Brightness invalide");
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
    Serial.print("[PUBNUB-ROUTE] set-bedtime-config: Effet sauvegardé: ");
    Serial.println(config.bedtime_effect);
  } else {
    // Pas d'effet fourni, utiliser "none" pour couleur fixe
    strcpy(config.bedtime_effect, "none");
  }
  
  // Sauvegarder weekdaySchedule si présent
  if (hasWeekdaySchedule) {
    // Sérialiser le weekdaySchedule en JSON string
    String scheduleStr;
    serializeJson(weekdayScheduleObj, scheduleStr);
    
    // Vérifier que la taille ne dépasse pas la limite
    if (scheduleStr.length() < sizeof(config.bedtime_weekdaySchedule)) {
      strncpy(config.bedtime_weekdaySchedule, scheduleStr.c_str(), sizeof(config.bedtime_weekdaySchedule) - 1);
      config.bedtime_weekdaySchedule[sizeof(config.bedtime_weekdaySchedule) - 1] = '\0';
      Serial.print("[PUBNUB-ROUTE] set-bedtime-config: weekdaySchedule sauvegardé: ");
      Serial.println(scheduleStr);
    } else {
      Serial.println("[PUBNUB-ROUTE] set-bedtime-config: weekdaySchedule trop grand, tronqué");
      strncpy(config.bedtime_weekdaySchedule, scheduleStr.c_str(), sizeof(config.bedtime_weekdaySchedule) - 1);
      config.bedtime_weekdaySchedule[sizeof(config.bedtime_weekdaySchedule) - 1] = '\0';
    }
  } else {
    // Pas de weekdaySchedule, garder la valeur existante ou mettre un objet vide
    if (strlen(config.bedtime_weekdaySchedule) == 0) {
      strcpy(config.bedtime_weekdaySchedule, "{}");
    }
  }
  
  // Sauvegarder sur la SD
  if (SDManager::saveConfig(config)) {
    Serial.print("[PUBNUB-ROUTE] set-bedtime-config: Configuration sauvegardée - RGB(");
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
    
    // Déclencher automatiquement le test avec la nouvelle configuration
    Serial.println("[PUBNUB-ROUTE] set-bedtime-config: Déclenchement automatique du test...");
    
    // Créer un JsonObject avec les paramètres pour le test
    // Utiliser les valeurs qui viennent d'être sauvegardées dans la config
    StaticJsonDocument<512> testJson;
    JsonObject testParams = testJson.createNestedObject("params");
    
    // Utiliser les valeurs sauvegardées dans la config (qui viennent d'être mises à jour)
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
      Serial.println("[PUBNUB-ROUTE] set-bedtime-config: Test automatique démarré avec succès");
    } else {
      Serial.println("[PUBNUB-ROUTE] set-bedtime-config: Erreur lors du démarrage du test automatique");
    }
    
    return true;
  } else {
    Serial.println("[PUBNUB-ROUTE] set-bedtime-config: Erreur lors de la sauvegarde");
    return false;
  }
}

bool ModelDreamPubNubRoutes::handleStartTestWakeup(const JsonObject& json) {
  // Format: { "action": "start-test-wakeup", "params": { "colorR": 255, "colorG": 200, "colorB": 100, "brightness": 50 } }
  // Démarre le test de la couleur et luminosité avec les paramètres fournis
  
  Serial.println("[PUBNUB-ROUTE] start-test-wakeup: Démarrage/mise à jour du test...");
  
  // Sauvegarder l'état actif pour savoir si c'est une mise à jour ou un nouveau test
  bool wasAlreadyActive = testWakeupActive;
  
  // Récupérer les paramètres
  int colorR = -1;
  int colorG = -1;
  int colorB = -1;
  int brightness = -1;
  
  Serial.print("[PUBNUB-ROUTE] start-test-wakeup: Message JSON reçu - ");
  serializeJson(json, Serial);
  Serial.println();
  
  // Nouvelle syntaxe avec params
  if (json["params"].is<JsonObject>()) {
    JsonObject params = json["params"].as<JsonObject>();
    Serial.println("[PUBNUB-ROUTE] start-test-wakeup: Utilisation de la syntaxe avec params");
    if (params["colorR"].is<int>()) colorR = params["colorR"].as<int>();
    if (params["colorG"].is<int>()) colorG = params["colorG"].as<int>();
    if (params["colorB"].is<int>()) colorB = params["colorB"].as<int>();
    if (params["brightness"].is<int>()) brightness = params["brightness"].as<int>();
    
    Serial.print("[PUBNUB-ROUTE] start-test-wakeup: Paramètres extraits - RGB(");
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
    Serial.println("[PUBNUB-ROUTE] start-test-wakeup: Utilisation de la syntaxe directe (legacy)");
    if (json["colorR"].is<int>()) colorR = json["colorR"].as<int>();
    if (json["colorG"].is<int>()) colorG = json["colorG"].as<int>();
    if (json["colorB"].is<int>()) colorB = json["colorB"].as<int>();
    if (json["brightness"].is<int>()) brightness = json["brightness"].as<int>();
  }
  
  // Valider les paramètres
  if (colorR < 0 || colorR > 255 || colorG < 0 || colorG > 255 || colorB < 0 || colorB > 255) {
    Serial.println("[PUBNUB-ROUTE] start-test-wakeup: Couleur invalide");
    return false;
  }
  
  if (brightness < 0 || brightness > 100) {
    Serial.println("[PUBNUB-ROUTE] start-test-wakeup: Brightness invalide");
    return false;
  }
  
  // Convertir brightness de 0-100 vers 0-255
  uint8_t brightnessValue = (brightness * 255 + 50) / 100;
  
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
    Serial.println("[PUBNUB-ROUTE] start-test-wakeup: Test déjà actif, timeout de 15 secondes réinitialisé");
  } else {
    Serial.println("[PUBNUB-ROUTE] start-test-wakeup: Nouveau test démarré, timeout de 15 secondes initialisé");
  }
  
  Serial.print("[PUBNUB-ROUTE] start-test-wakeup: Test démarré - Couleur RGB(");
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

bool ModelDreamPubNubRoutes::handleStopTestWakeup(const JsonObject& json) {
  // Format: { "action": "stop-test-wakeup" }
  // Arrête le test de l'heure de réveil
  
  if (!testWakeupActive) {
    Serial.println("[PUBNUB-ROUTE] stop-test-wakeup: Aucun test actif");
    return false;
  }
  
  Serial.println("[PUBNUB-ROUTE] stop-test-wakeup: Arrêt du test");
  
  // Éteindre les LEDs
  LEDManager::clear();
  
  // Désactiver le test
  testWakeupActive = false;
  testWakeupStartTime = 0;
  
  return true;
}

bool ModelDreamPubNubRoutes::handleSetWakeupConfig(const JsonObject& json) {
  // Format: { "action": "set-wakeup-config", "params": { "colorR": 255, "colorG": 200, "colorB": 100, "brightness": 50, "weekdaySchedule": {...} } }
  // Sauvegarde la configuration de l'heure de réveil sur la carte SD
  
  Serial.println("[PUBNUB-ROUTE] set-wakeup-config: Sauvegarde de la configuration...");
  
  if (!SDManager::isAvailable()) {
    Serial.println("[PUBNUB-ROUTE] set-wakeup-config: Carte SD non disponible");
    return false;
  }
  
  // Récupérer les paramètres
  int colorR = -1;
  int colorG = -1;
  int colorB = -1;
  int brightness = -1;
  JsonObject weekdayScheduleObj;
  bool hasWeekdaySchedule = false;
  
  // Nouvelle syntaxe avec params
  if (json["params"].is<JsonObject>()) {
    JsonObject params = json["params"].as<JsonObject>();
    Serial.println("[PUBNUB-ROUTE] set-wakeup-config: Utilisation de la syntaxe avec params");
    
    if (params["colorR"].is<int>()) colorR = params["colorR"].as<int>();
    if (params["colorG"].is<int>()) colorG = params["colorG"].as<int>();
    if (params["colorB"].is<int>()) colorB = params["colorB"].as<int>();
    if (params["brightness"].is<int>()) brightness = params["brightness"].as<int>();
    
    // Récupérer weekdaySchedule si présent
    if (params["weekdaySchedule"].is<JsonObject>()) {
      weekdayScheduleObj = params["weekdaySchedule"].as<JsonObject>();
      hasWeekdaySchedule = true;
    }
  }
  // Syntaxe directe (legacy)
  else {
    Serial.println("[PUBNUB-ROUTE] set-wakeup-config: Utilisation de la syntaxe directe (legacy)");
    if (json["colorR"].is<int>()) colorR = json["colorR"].as<int>();
    if (json["colorG"].is<int>()) colorG = json["colorG"].as<int>();
    if (json["colorB"].is<int>()) colorB = json["colorB"].as<int>();
    if (json["brightness"].is<int>()) brightness = json["brightness"].as<int>();
    
    // Récupérer weekdaySchedule si présent
    if (json["weekdaySchedule"].is<JsonObject>()) {
      weekdayScheduleObj = json["weekdaySchedule"].as<JsonObject>();
      hasWeekdaySchedule = true;
    }
  }
  
  // Valider les paramètres
  if (colorR < 0 || colorR > 255 || colorG < 0 || colorG > 255 || colorB < 0 || colorB > 255) {
    Serial.println("[PUBNUB-ROUTE] set-wakeup-config: Couleur invalide");
    return false;
  }
  
  if (brightness < 0 || brightness > 100) {
    Serial.println("[PUBNUB-ROUTE] set-wakeup-config: Brightness invalide");
    return false;
  }
  
  // Charger la configuration actuelle depuis la SD
  SDConfig config = SDManager::getConfig();
  
  // Mettre à jour les champs wakeup
  config.wakeup_colorR = (uint8_t)colorR;
  config.wakeup_colorG = (uint8_t)colorG;
  config.wakeup_colorB = (uint8_t)colorB;
  config.wakeup_brightness = (uint8_t)brightness;
  
  // Sauvegarder weekdaySchedule si présent
  if (hasWeekdaySchedule) {
    // Sérialiser le weekdaySchedule en JSON string
    String scheduleStr;
    serializeJson(weekdayScheduleObj, scheduleStr);
    
    // Vérifier que la taille ne dépasse pas la limite
    if (scheduleStr.length() < sizeof(config.wakeup_weekdaySchedule)) {
      strncpy(config.wakeup_weekdaySchedule, scheduleStr.c_str(), sizeof(config.wakeup_weekdaySchedule) - 1);
      config.wakeup_weekdaySchedule[sizeof(config.wakeup_weekdaySchedule) - 1] = '\0';
      Serial.print("[PUBNUB-ROUTE] set-wakeup-config: weekdaySchedule sauvegardé: ");
      Serial.println(scheduleStr);
    } else {
      Serial.println("[PUBNUB-ROUTE] set-wakeup-config: weekdaySchedule trop grand, tronqué");
      strncpy(config.wakeup_weekdaySchedule, scheduleStr.c_str(), sizeof(config.wakeup_weekdaySchedule) - 1);
      config.wakeup_weekdaySchedule[sizeof(config.wakeup_weekdaySchedule) - 1] = '\0';
    }
  } else {
    // Pas de weekdaySchedule, garder la valeur existante ou mettre un objet vide
    if (strlen(config.wakeup_weekdaySchedule) == 0) {
      strcpy(config.wakeup_weekdaySchedule, "{}");
    }
  }
  
  // Sauvegarder sur la SD
  if (SDManager::saveConfig(config)) {
    Serial.print("[PUBNUB-ROUTE] set-wakeup-config: Configuration sauvegardée - RGB(");
    Serial.print(colorR);
    Serial.print(",");
    Serial.print(colorG);
    Serial.print(",");
    Serial.print(colorB);
    Serial.print("), Brightness: ");
    Serial.print(brightness);
    Serial.print("%");
    if (hasWeekdaySchedule) {
      Serial.print(", weekdaySchedule: ");
      Serial.println(config.wakeup_weekdaySchedule);
    } else {
      Serial.println();
    }
    
    // Recharger la configuration dans le WakeupManager
    WakeupManager::reloadConfig();
    
    // Déclencher automatiquement le test avec la nouvelle configuration
    Serial.println("[PUBNUB-ROUTE] set-wakeup-config: Déclenchement automatique du test...");
    
    // Créer un JsonObject avec les paramètres pour le test
    // Utiliser les valeurs sauvegardées dans la config
    StaticJsonDocument<512> testJson;
    JsonObject testParams = testJson.createNestedObject("params");
    
    // Utiliser les valeurs sauvegardées dans la config
    testParams["colorR"] = config.wakeup_colorR;
    testParams["colorG"] = config.wakeup_colorG;
    testParams["colorB"] = config.wakeup_colorB;
    testParams["brightness"] = config.wakeup_brightness;
    
    // Appeler handleStartTestWakeup avec les paramètres
    if (handleStartTestWakeup(testJson.as<JsonObject>())) {
      Serial.println("[PUBNUB-ROUTE] set-wakeup-config: Test automatique démarré avec succès");
    } else {
      Serial.println("[PUBNUB-ROUTE] set-wakeup-config: Erreur lors du démarrage du test automatique");
    }
    
    return true;
  } else {
    Serial.println("[PUBNUB-ROUTE] set-wakeup-config: Erreur lors de la sauvegarde");
    return false;
  }
}

void ModelDreamPubNubRoutes::printRoutes() {
  Serial.println("");
  Serial.println("========== Routes PubNub Dream ==========");
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
  Serial.println("==========================================");
}
