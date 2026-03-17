#include "model_pubnub_routes.h"
#include "app_config.h"
#include "common/config/default_config.h"
#include "../config/default_config.h"
#include "common/managers/log/log_manager.h"
#include "common/managers/sd/sd_manager.h"
#include "common/managers/pubnub/pubnub_manager.h"
#include "common/managers/wifi/wifi_manager.h"
#include "common/managers/init/init_manager.h"
#include "common/managers/rtc/rtc_manager.h"
#include "common/utils/mac_utils.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <cstring>
#include <cstdint>

/**
 * Implémentation des routes PubNub pour Sound
 */

// Forward declarations
static bool handleGetInfo(const JsonObject& json);
static bool handleBrightness(const JsonObject& json);

bool ModelSoundPubNubRoutes::processMessage(const JsonObject& json) {
  // Vérifier que l'action est présente
  if (!json["action"].is<const char*>()) {
    Serial.println("[PUBNUB-ROUTE-SOUND] Erreur: action manquante");
    return false;
  }

  const char* action = json["action"].as<const char*>();
  if (action == nullptr) {
    Serial.println("[PUBNUB-ROUTE-SOUND] Erreur: action est null");
    return false;
  }

  Serial.print("[PUBNUB-ROUTE-SOUND] Action reçue: ");
  Serial.println(action);

  // Router vers le bon handler
  if (strcmp(action, "get-info") == 0 || strcmp(action, "getinfo") == 0) {
    return handleGetInfo(json);
  }
  else if (strcmp(action, "brightness") == 0) {
    return handleBrightness(json);
  }

  Serial.println("[PUBNUB-ROUTE-SOUND] Action inconnue");
  return false;
}

void ModelSoundPubNubRoutes::printRoutes() {
  Serial.println("\n=== Routes PubNub - Sound ===");
  Serial.println("  - get-info: Récupérer les infos (stockage, firmware, MAC, etc.)");
  Serial.println("  - brightness: Changer la luminosité");
  Serial.println("\nExemples:");
  Serial.println("  { \"action\": \"get-info\" }");
  Serial.println("  { \"action\": \"brightness\", \"value\": 75 }");
}

/**
 * Récupère et publie les informations complètes du Kidoo Sound
 */
static bool handleGetInfo(const JsonObject& json) {
  Serial.println("[PUBNUB-ROUTE-SOUND] get-info: Préparation des informations...");

  // Récupérer les infos de stockage SD
  uint64_t totalBytes = 0;
  uint64_t freeBytes = 0;
  uint64_t usedBytes = 0;

  if (SDManager::isAvailable()) {
    totalBytes = SDManager::getTotalSpace();
    usedBytes = SDManager::getUsedSpace();
    freeBytes = SDManager::getFreeSpace();
    Serial.printf("[PUBNUB-ROUTE-SOUND] Storage: total=%llu, used=%llu, free=%llu\n", totalBytes, usedBytes, freeBytes);
  } else {
    Serial.println("[PUBNUB-ROUTE-SOUND] SD non disponible");
  }

  // Récupérer la MAC address WiFi
  char macStr[18];
  if (!getMacAddressString(macStr, sizeof(macStr), ESP_MAC_WIFI_STA)) {
    strcpy(macStr, "00:00:00:00:00:00");
  }

  // Récupérer la version firmware (macro définie dans default_config.h)
  const char* fwVersion = FIRMWARE_VERSION;

  // Récupérer la luminosité actuelle (valeur par défaut pour l'instant)
  int brightness = 100;

  // Construire le JSON de réponse pour Sound
  char infoJson[512];
  snprintf(infoJson, sizeof(infoJson),
    "{"
      "\"type\":\"info\","
      "\"device\":\"Kidoo Sound\","
      "\"mac\":\"%s\","
      "\"model\":\"sound\","
      "\"firmwareVersion\":\"%s\","
      "\"brightness\":%d,"
      "\"storage\":{"
        "\"total\":%llu,"
        "\"free\":%llu,"
        "\"used\":%llu"
      "}"
    "}",
    macStr,
    fwVersion,
    brightness,
    totalBytes,
    freeBytes,
    usedBytes
  );

  // Publier via PubNub
  if (PubNubManager::publish(infoJson)) {
    Serial.println("[PUBNUB-ROUTE-SOUND] get-info: Infos publiées avec succès");
    return true;
  } else {
    Serial.println("[PUBNUB-ROUTE-SOUND] get-info: Erreur lors de la publication");
    return false;
  }
}

/**
 * Gère le changement de luminosité
 */
static bool handleBrightness(const JsonObject& json) {
  if (!json["value"].is<int>()) {
    Serial.println("[PUBNUB-ROUTE-SOUND] brightness: value manquante");
    return false;
  }

  int brightness = json["value"];
  if (brightness < 0 || brightness > 100) {
    Serial.println("[PUBNUB-ROUTE-SOUND] brightness: valeur hors limites (0-100)");
    return false;
  }

  Serial.printf("[PUBNUB-ROUTE-SOUND] brightness: %d%%\n", brightness);
  // TODO: Appliquer la luminosité au modèle Sound quand le manager existe

  return true;
}
