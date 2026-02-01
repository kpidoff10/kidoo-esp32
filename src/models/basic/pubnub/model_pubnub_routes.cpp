#include "model_pubnub_routes.h"
#include "../../common/managers/led/led_manager.h"
#include "../../common/managers/init/init_manager.h"
#include "../../common/managers/wifi/wifi_manager.h"
#include "../../common/managers/pubnub/pubnub_manager.h"
#include "../../common/managers/sd/sd_manager.h"
#include "../../common/managers/nfc/nfc_manager.h"
#include "../../common/utils/mac_utils.h"

/**
 * Routes PubNub spécifiques au modèle Kidoo Basic
 */

bool ModelBasicPubNubRoutes::processMessage(const JsonObject& json) {
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
  
  Serial.print("[PUBNUB-ROUTE] Action inconnue: ");
  Serial.println(action);
  return false;
}

bool ModelBasicPubNubRoutes::handleGetInfo(const JsonObject& json) {
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
      "\"model\":\"basic\","
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

bool ModelBasicPubNubRoutes::handleBrightness(const JsonObject& json) {
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

bool ModelBasicPubNubRoutes::handleSleepTimeout(const JsonObject& json) {
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

bool ModelBasicPubNubRoutes::handleReboot(const JsonObject& json) {
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

bool ModelBasicPubNubRoutes::handleLed(const JsonObject& json) {
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

void ModelBasicPubNubRoutes::printRoutes() {
  Serial.println("");
  Serial.println("========== Routes PubNub Basic ==========");
  Serial.println("{ \"action\": \"get-info\" }");
  Serial.println("{ \"action\": \"brightness\", \"params\": { \"value\": 1-100 } }");
  Serial.println("{ \"action\": \"sleep-timeout\", \"params\": { \"value\": 0|5000-300000 } }");
  Serial.println("{ \"action\": \"reboot\", \"params\": { \"delay\": ms } }");
  Serial.println("{ \"action\": \"led\", \"color\": \"#RRGGBB\" }");
  Serial.println("{ \"action\": \"led\", \"effect\": \"none|pulse|rotate|rainbow|glossy|off\" }");
  Serial.println("==========================================");
}
