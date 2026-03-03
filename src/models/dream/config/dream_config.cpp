#include "dream_config.h"
#include "common/managers/sd/sd_manager.h"
#include <ArduinoJson.h>
#include <SD.h>
#include <cstring>

static const char* CONFIG_PATH = "/config.json";

void DreamConfigManager::initDefaultConfig(DreamConfig* config) {
  if (config == nullptr) return;
  config->nighttime_alert_enabled = false;
  config->default_color_r = 255;      // Red by default
  config->default_color_g = 0;
  config->default_color_b = 0;
  config->default_brightness = 50;    // 50% brightness
  strcpy(config->default_effect, ""); // No effect (solid color)
}

DreamConfig DreamConfigManager::getConfig() {
  DreamConfig config;
  initDefaultConfig(&config);

  if (!SDManager::isAvailable() || !SDManager::configFileExists()) {
    return config;
  }

  File configFile = SD.open(CONFIG_PATH, FILE_READ);
  if (!configFile) return config;

  const size_t maxSize = 4096;
  size_t fileSize = configFile.size();
  if (fileSize == 0 || fileSize > maxSize) {
    configFile.close();
    return config;
  }

  char* jsonBuffer = new char[fileSize + 1];
  if (!jsonBuffer) {
    configFile.close();
    return config;
  }
  size_t bytesRead = configFile.readBytes(jsonBuffer, fileSize);
  jsonBuffer[bytesRead] = '\0';
  configFile.close();

  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  StaticJsonDocument<maxSize> doc;
  #pragma GCC diagnostic pop
  DeserializationError error = deserializeJson(doc, jsonBuffer);
  delete[] jsonBuffer;

  if (error) return config;

  JsonObject dream = doc["dream"];
  if (dream.isNull()) return config;

  if (dream["nighttime_alert_enabled"].is<bool>()) {
    config.nighttime_alert_enabled = dream["nighttime_alert_enabled"].as<bool>();
  }

  // Load default color configuration
  if (dream["default_color_r"].is<int>()) {
    config.default_color_r = dream["default_color_r"].as<uint8_t>();
  }
  if (dream["default_color_g"].is<int>()) {
    config.default_color_g = dream["default_color_g"].as<uint8_t>();
  }
  if (dream["default_color_b"].is<int>()) {
    config.default_color_b = dream["default_color_b"].as<uint8_t>();
  }
  if (dream["default_brightness"].is<int>()) {
    config.default_brightness = dream["default_brightness"].as<uint8_t>();
  }
  if (dream["default_effect"].is<const char*>()) {
    strncpy(config.default_effect, dream["default_effect"].as<const char*>(), sizeof(config.default_effect) - 1);
    config.default_effect[sizeof(config.default_effect) - 1] = '\0';
  }

  return config;
}

bool DreamConfigManager::saveConfig(const DreamConfig& config) {
  if (!SDManager::isAvailable()) return false;

  const char* path = CONFIG_PATH;
  const size_t maxSize = 4096;

  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  StaticJsonDocument<maxSize> doc;
  #pragma GCC diagnostic pop

  if (SD.exists(path)) {
    File configFile = SD.open(path, FILE_READ);
    if (configFile) {
      size_t fileSize = configFile.size();
      if (fileSize > 0 && fileSize <= maxSize) {
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

  JsonObject dream = doc["dream"].to<JsonObject>();
  dream["nighttime_alert_enabled"] = config.nighttime_alert_enabled;
  dream["default_color_r"] = config.default_color_r;
  dream["default_color_g"] = config.default_color_g;
  dream["default_color_b"] = config.default_color_b;
  dream["default_brightness"] = config.default_brightness;
  dream["default_effect"] = config.default_effect;

  File configFile = SD.open(path, FILE_WRITE);
  if (!configFile) return false;
  size_t bytesWritten = serializeJson(doc, configFile);
  configFile.close();

  return bytesWritten > 0;
}
