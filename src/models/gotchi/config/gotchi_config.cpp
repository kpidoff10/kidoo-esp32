#include "gotchi_config.h"
#include "common/managers/sd/sd_manager.h"
#include <ArduinoJson.h>
#include <SD.h>
#include <Arduino.h>
#include <ctime>
#include <cstring>

static const char* CONFIG_PATH = "/config.json";

void GotchiConfigManager::initDefault(GotchiStatsConfig* stats) {
  if (!stats) return;
  stats->valid        = false;
  stats->hunger       = 70.0f;
  stats->energy       = 80.0f;
  stats->happiness    = 60.0f;
  stats->health       = 90.0f;
  stats->hygiene      = 70.0f;
  stats->boredom      = 20.0f;
  stats->irritability = 0.0f;
  stats->ageMinutes   = 0;
  stats->lastSavedAt  = 0;
}

GotchiStatsConfig GotchiConfigManager::getStats() {
  GotchiStatsConfig stats;
  initDefault(&stats);

  if (!SDManager::isAvailable() || !SDManager::configFileExists()) {
    return stats;
  }

  File configFile = SD.open(CONFIG_PATH, FILE_READ);
  if (!configFile) return stats;

  const size_t maxSize = 4096;
  size_t fileSize = configFile.size();
  if (fileSize == 0 || fileSize > maxSize) {
    configFile.close();
    return stats;
  }

  char jsonBuffer[4096];
  size_t bytesRead = configFile.readBytes(jsonBuffer, fileSize);
  jsonBuffer[bytesRead] = '\0';
  configFile.close();

  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  StaticJsonDocument<maxSize> doc;
  #pragma GCC diagnostic pop
  if (deserializeJson(doc, jsonBuffer)) return stats;

  JsonObject gotchi = doc["gotchi"];
  if (gotchi.isNull()) return stats;

  if (gotchi["hunger"].is<float>())       stats.hunger       = gotchi["hunger"].as<float>();
  if (gotchi["energy"].is<float>())       stats.energy       = gotchi["energy"].as<float>();
  if (gotchi["happiness"].is<float>())    stats.happiness    = gotchi["happiness"].as<float>();
  if (gotchi["health"].is<float>())       stats.health       = gotchi["health"].as<float>();
  if (gotchi["hygiene"].is<float>())      stats.hygiene      = gotchi["hygiene"].as<float>();
  if (gotchi["boredom"].is<float>())      stats.boredom      = gotchi["boredom"].as<float>();
  if (gotchi["irritability"].is<float>()) stats.irritability = gotchi["irritability"].as<float>();
  if (gotchi["ageMinutes"].is<uint32_t>()) stats.ageMinutes  = gotchi["ageMinutes"].as<uint32_t>();
  if (gotchi["lastSavedAt"].is<uint32_t>()) stats.lastSavedAt = gotchi["lastSavedAt"].as<uint32_t>();

  stats.valid = true;
  Serial.printf("[GOTCHI_CONFIG] Loaded stats: hunger=%.0f energy=%.0f happy=%.0f health=%.0f hygiene=%.0f age=%lum lastSaved=%lu\n",
    stats.hunger, stats.energy, stats.happiness, stats.health, stats.hygiene,
    stats.ageMinutes, stats.lastSavedAt);
  return stats;
}

bool GotchiConfigManager::saveStats(const GotchiStatsConfig& stats) {
  if (!SDManager::isAvailable()) return false;

  const size_t maxSize = 4096;

  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  StaticJsonDocument<maxSize> doc;
  #pragma GCC diagnostic pop

  // Lire l'existant pour merger (preserve les autres cles)
  if (SD.exists(CONFIG_PATH)) {
    File configFile = SD.open(CONFIG_PATH, FILE_READ);
    if (configFile) {
      size_t fileSize = configFile.size();
      if (fileSize > 0 && fileSize <= maxSize) {
        char jsonBuffer[4096];
        size_t bytesRead = configFile.readBytes(jsonBuffer, fileSize);
        jsonBuffer[bytesRead] = '\0';
        deserializeJson(doc, jsonBuffer);
      }
      configFile.close();
    }
  }

  JsonObject gotchi = doc["gotchi"].to<JsonObject>();
  gotchi["hunger"]       = stats.hunger;
  gotchi["energy"]       = stats.energy;
  gotchi["happiness"]    = stats.happiness;
  gotchi["health"]       = stats.health;
  gotchi["hygiene"]      = stats.hygiene;
  gotchi["boredom"]      = stats.boredom;
  gotchi["irritability"] = stats.irritability;
  gotchi["ageMinutes"]   = stats.ageMinutes;

  // Timestamp courant (RTC). 0 si pas de RTC valide → pas de decay offline.
  time_t now = time(nullptr);
  gotchi["lastSavedAt"] = (now > 100000) ? (uint32_t)now : 0;

  // Reecrire (FILE_WRITE est mode overwrite/create sur ESP32)
  SD.remove(CONFIG_PATH);
  File configFile = SD.open(CONFIG_PATH, FILE_WRITE);
  if (!configFile) return false;
  size_t bytesWritten = serializeJson(doc, configFile);
  configFile.close();

  return bytesWritten > 0;
}
