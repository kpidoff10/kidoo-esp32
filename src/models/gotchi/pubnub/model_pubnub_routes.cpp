#include "model_pubnub_routes.h"
#include "../../../common/managers/led/led_manager.h"
#include "../../../common/managers/init/init_manager.h"
#include "../../../common/managers/wifi/wifi_manager.h"
#include "../../../common/managers/pubnub/pubnub_manager.h"
#include "../../../common/managers/sd/sd_manager.h"
#include "../../../common/managers/ota/ota_manager.h"

/**
 * Routes PubNub spécifiques au modèle Kidoo Gotchi
 */

bool ModelGotchiPubNubRoutes::processMessage(const JsonObject& json) {
  if (!json["action"].is<const char*>()) {
    return false;
  }

  const char* action = json["action"].as<const char*>();
  if (action == nullptr) {
    return false;
  }

  Serial.print("[PUBNUB-ROUTE] Action: ");
  Serial.println(action);

  if (strcmp(action, "brightness") == 0) {
    return handleBrightness(json);
  }
  if (strcmp(action, "sleep") == 0) {
    return handleSleep(json);
  }
  if (strcmp(action, "led") == 0) {
    return handleLed(json);
  }
  if (strcmp(action, "status") == 0) {
    return handleStatus(json);
  }
  if (strcmp(action, "firmware-update") == 0) {
    return handleFirmwareUpdate(json);
  }

  return false;
}

bool ModelGotchiPubNubRoutes::handleBrightness(const JsonObject& json) {
  if (!json["value"].is<int>()) {
    return false;
  }

  int value = json["value"].as<int>();
  if (value < 0) value = 0;
  if (value > 100) value = 100;

  uint8_t brightness = (value * 255) / 100;

  if (LEDManager::setBrightness(brightness)) {
    SDConfig config = SDManager::getConfig();
    config.led_brightness = brightness;
    SDManager::saveConfig(config);
    Serial.print("[PUBNUB-ROUTE] Luminosite: ");
    Serial.print(value);
    Serial.println("%");
    return true;
  }

  return false;
}

bool ModelGotchiPubNubRoutes::handleSleep(const JsonObject& json) {
  if (json["enabled"].is<bool>() && !json["enabled"].as<bool>()) {
    SDConfig config = SDManager::getConfig();
    config.sleep_timeout_ms = 0;
    SDManager::saveConfig(config);
    Serial.println("[PUBNUB-ROUTE] Sleep mode desactive");
    return true;
  }

  if (json["timeout"].is<unsigned long>()) {
    uint32_t timeout = json["timeout"].as<uint32_t>();
    if (timeout > 0 && timeout < 5000) timeout = 5000;

    SDConfig config = SDManager::getConfig();
    config.sleep_timeout_ms = timeout;
    SDManager::saveConfig(config);

    Serial.print("[PUBNUB-ROUTE] Sleep timeout: ");
    Serial.println(timeout);
    return true;
  }

  return false;
}

bool ModelGotchiPubNubRoutes::handleLed(const JsonObject& json) {
  bool handled = false;

  if (json["color"].is<const char*>()) {
    const char* colorStr = json["color"].as<const char*>();
    uint8_t r = 0, g = 0, b = 0;

    if (colorStr[0] == '#' && strlen(colorStr) == 7) {
      long hexColor = strtol(colorStr + 1, nullptr, 16);
      r = (hexColor >> 16) & 0xFF;
      g = (hexColor >> 8) & 0xFF;
      b = hexColor & 0xFF;
    } else if (strcmp(colorStr, "red") == 0) { r = 255; g = 0; b = 0; }
    else if (strcmp(colorStr, "green") == 0) { r = 0; g = 255; b = 0; }
    else if (strcmp(colorStr, "blue") == 0) { r = 0; g = 0; b = 255; }
    else if (strcmp(colorStr, "white") == 0) { r = 255; g = 255; b = 255; }
    else if (strcmp(colorStr, "off") == 0) { r = 0; g = 0; b = 0; }

    LEDManager::setColor(r, g, b);
    handled = true;
  }

  if (json["effect"].is<const char*>()) {
    const char* effectStr = json["effect"].as<const char*>();
    LEDEffect effect = LED_EFFECT_NONE;

    if (strcmp(effectStr, "none") == 0 || strcmp(effectStr, "solid") == 0) effect = LED_EFFECT_NONE;
    else if (strcmp(effectStr, "pulse") == 0) effect = LED_EFFECT_PULSE;
    else if (strcmp(effectStr, "rotate") == 0) effect = LED_EFFECT_ROTATE;
    else if (strcmp(effectStr, "rainbow") == 0) effect = LED_EFFECT_RAINBOW;
    else if (strcmp(effectStr, "glossy") == 0) effect = LED_EFFECT_GLOSSY;
    else if (strcmp(effectStr, "off") == 0) {
      LEDManager::clear();
      return true;
    }

    LEDManager::setEffect(effect);
    handled = true;
  }

  return handled;
}

bool ModelGotchiPubNubRoutes::handleStatus(const JsonObject& json) {
  SDConfig config = SDManager::getConfig();

  char statusJson[256];
  snprintf(statusJson, sizeof(statusJson),
    "{\"type\":\"status\",\"device\":\"%s\",\"ip\":\"%s\",\"brightness\":%d}",
    DEFAULT_DEVICE_NAME,
    WiFiManager::getLocalIP().c_str(),
    (LEDManager::getCurrentBrightness() * 100) / 255
  );

  PubNubManager::publish(statusJson);
  return true;
}

bool ModelGotchiPubNubRoutes::handleFirmwareUpdate(const JsonObject& json) {
  const char* version = nullptr;
  if (json["params"].is<JsonObject>()) {
    JsonObject params = json["params"].as<JsonObject>();
    if (params["version"].is<const char*>()) version = params["version"].as<const char*>();
  }
  if (json["version"].is<const char*>()) version = json["version"].as<const char*>();
  if (version == nullptr || strlen(version) == 0) {
    Serial.println("[PUBNUB-ROUTE] firmware-update: version manquante");
    return false;
  }
  Serial.print("[PUBNUB-ROUTE] firmware-update: version cible ");
  Serial.println(version);
#ifdef HAS_WIFI
  return OTAManager::startUpdateTask(version);
#else
  Serial.println("[PUBNUB-ROUTE] firmware-update: WiFi non disponible sur ce build");
  return false;
#endif
}

void ModelGotchiPubNubRoutes::printRoutes() {
  Serial.println("");
  Serial.println("========== Routes PubNub Gotchi ==========");
  Serial.println("{ \"action\": \"brightness\", \"value\": 0-100 }");
  Serial.println("{ \"action\": \"sleep\", \"timeout\": ms }");
  Serial.println("{ \"action\": \"led\", \"color\": \"#RRGGBB\" }");
  Serial.println("{ \"action\": \"led\", \"effect\": \"none|pulse|rotate|rainbow|glossy|off\" }");
  Serial.println("{ \"action\": \"status\" }");
  Serial.println("{ \"action\": \"firmware-update\", \"version\": \"1.0.1\" }");
  Serial.println("==========================================");
}
