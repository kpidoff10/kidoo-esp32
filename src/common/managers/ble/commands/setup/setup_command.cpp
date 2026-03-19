#include "setup_command.h"
#include <ArduinoJson.h>
#include "../../../init/init_manager.h"
#include "../../../sd/sd_manager.h"
#include "../../../wifi/wifi_manager.h"
#include "../../../led/led_manager.h"
#include "../../../device_key/device_key_manager.h"
#include "../ble_command_handler.h"

static bool s_setupAsyncPending = false;

bool BLESetupCommand::isAsyncPending() {
  return s_setupAsyncPending;
}

bool BLESetupCommand::isValid(const String& jsonData) {
  if (jsonData.length() == 0) return false;

  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  StaticJsonDocument<512> doc;
  #pragma GCC diagnostic pop
  DeserializationError error = deserializeJson(doc, jsonData);
  if (error) {
    Serial.printf("[SETUP] Erreur parsing JSON: %s\n", error.c_str());
    return false;
  }

  if (!doc["command"].is<String>() || doc["command"] != "setup") return false;
  if (!doc["ssid"].is<String>()) return false;
  return true;
}

struct SetupAsyncContext {
  char ssid[64];
  char password[64];
};

static void onSetupConnectComplete(bool success, void* userData) {
  SetupAsyncContext* ctx = static_cast<SetupAsyncContext*>(userData);
  bool wifiConnected = success;

  if (success) {
    SDConfig config = InitManager::getConfig();
    strncpy(config.wifi_ssid, ctx->ssid, sizeof(config.wifi_ssid) - 1);
    config.wifi_ssid[sizeof(config.wifi_ssid) - 1] = '\0';
    strncpy(config.wifi_password, ctx->password, sizeof(config.wifi_password) - 1);
    config.wifi_password[sizeof(config.wifi_password) - 1] = '\0';

    if (!SDManager::isAvailable()) {
      Serial.println("[SETUP] ERREUR: Carte SD non disponible");
      wifiConnected = false;
    } else if (!InitManager::updateConfig(config)) {
      Serial.println("[SETUP] ERREUR: Impossible de sauvegarder la configuration");
      wifiConnected = false;
    }

    if (wifiConnected) {
      char pubKeyB64[48];
      if (!DeviceKeyManager::getOrCreatePublicKeyBase64(pubKeyB64, sizeof(pubKeyB64))) {
        Serial.println("[SETUP] ERREUR: Impossible de créer la clé device");
        wifiConnected = false;
      }
    }
  }

  delete ctx;
  s_setupAsyncPending = false;
  BLECommandHandler::sendSetupCompletionResponse(success && wifiConnected, wifiConnected);

  WiFiManager::setSkipPostConnectActions(false);
  if (!wifiConnected) {
    WiFiManager::disconnect();
  }
}

bool BLESetupCommand::execute(const String& jsonData) {
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  StaticJsonDocument<512> doc;
  #pragma GCC diagnostic pop
  DeserializationError error = deserializeJson(doc, jsonData);
  if (error) {
    Serial.printf("[SETUP] Erreur parsing JSON: %s\n", error.c_str());
    return false;
  }

  String ssid = doc["ssid"] | "";
  String password = doc["password"] | "";
  ssid.trim();
  password.trim();

  if (ssid.length() == 0) {
    Serial.println("[SETUP] Erreur: SSID vide");
    return false;
  }
  if (ssid.length() >= 64) {
    Serial.println("[SETUP] Erreur: SSID trop long");
    return false;
  }
  if (password.length() >= 64) {
    Serial.println("[SETUP] Erreur: Mot de passe trop long");
    return false;
  }

  #ifdef HAS_LED
  if (LEDManager::isInitialized()) {
    LEDManager::setEffect(LED_EFFECT_RAINBOW);
  }
  #endif

  #ifdef HAS_WIFI
  if (!WiFiManager::isAvailable()) {
    Serial.println("[SETUP] ERREUR: WiFi non disponible");
    return false;
  }

  if (WiFiManager::isRetryThreadActive()) {
    WiFiManager::stopRetryThread();
    delay(200);
  }

  if (WiFiManager::isConnected()) {
    WiFiManager::disconnect();
    delay(500);
  }

  SetupAsyncContext* ctx = new SetupAsyncContext();
  strncpy(ctx->ssid, ssid.c_str(), sizeof(ctx->ssid) - 1);
  ctx->ssid[sizeof(ctx->ssid) - 1] = '\0';
  strncpy(ctx->password, password.c_str(), sizeof(ctx->password) - 1);
  ctx->password[sizeof(ctx->password) - 1] = '\0';

  s_setupAsyncPending = true;
  WiFiManager::setSkipPostConnectActions(true);
  WiFiManager::connectAsync(
    ctx->ssid,
    ctx->password[0] ? ctx->password : nullptr,
    20000,
    onSetupConnectComplete,
    ctx
  );
  return false;
  #else
  Serial.println("[SETUP] ERREUR: WiFi non disponible sur ce modele");
  return false;
  #endif
}
