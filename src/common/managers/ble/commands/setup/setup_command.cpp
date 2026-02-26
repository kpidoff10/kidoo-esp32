#include "setup_command.h"
#include <ArduinoJson.h>
#include "../../../init/init_manager.h"
#include "../../../sd/sd_manager.h"
#include "../../../wifi/wifi_manager.h"
#include "../../../led/led_manager.h"
#include "../ble_command_handler.h"

static bool s_setupAsyncPending = false;

bool BLESetupCommand::isAsyncPending() {
  return s_setupAsyncPending;
}

bool BLESetupCommand::isValid(const String& jsonData) {
  if (jsonData.length() == 0) {
    return false;
  }
  
  // Parser le JSON pour vérifier la structure
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  StaticJsonDocument<512> doc;
  #pragma GCC diagnostic pop
  DeserializationError error = deserializeJson(doc, jsonData);
  
  if (error) {
    Serial.print("[BLE-COMMAND] Erreur parsing JSON: ");
    Serial.println(error.c_str());
    return false;
  }
  
  // Vérifier que c'est bien la commande "setup"
  if (!doc["command"].is<String>() || doc["command"] != "setup") {
    return false;
  }
  
  // Vérifier que le SSID est présent
  if (!doc["ssid"].is<String>()) {
    return false;
  }
  
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
    Serial.println("[BLE-COMMAND] Connexion WiFi reussie! Sauvegarde de la configuration...");
    SDConfig config = InitManager::getConfig();
    strncpy(config.wifi_ssid, ctx->ssid, sizeof(config.wifi_ssid) - 1);
    config.wifi_ssid[sizeof(config.wifi_ssid) - 1] = '\0';
    strncpy(config.wifi_password, ctx->password, sizeof(config.wifi_password) - 1);
    config.wifi_password[sizeof(config.wifi_password) - 1] = '\0';
    
    if (!SDManager::isAvailable()) {
      Serial.println("[BLE-COMMAND] ERREUR: Carte SD non disponible");
      WiFiManager::disconnect();
      wifiConnected = false;
    } else if (!InitManager::updateConfig(config)) {
      Serial.println("[BLE-COMMAND] ERREUR: Impossible de sauvegarder la configuration");
      WiFiManager::disconnect();
      wifiConnected = false;
    } else {
      Serial.println("[BLE-COMMAND] Configuration WiFi sauvegardee avec succes!");
    }
  } else {
    Serial.println("[BLE-COMMAND] Echec de connexion WiFi - Configuration NON sauvegardee");
  }
  
  delete ctx;
  s_setupAsyncPending = false;
  BLECommandHandler::sendSetupCompletionResponse(success && wifiConnected, wifiConnected);
}

bool BLESetupCommand::execute(const String& jsonData) {
  Serial.println("[BLE-COMMAND] Execution de la commande 'setup' (non bloquant)");
  
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  StaticJsonDocument<512> doc;
  #pragma GCC diagnostic pop
  DeserializationError error = deserializeJson(doc, jsonData);
  
  if (error) {
    Serial.print("[BLE-COMMAND] Erreur parsing JSON: ");
    Serial.println(error.c_str());
    return false;
  }
  
  String ssid = doc["ssid"] | "";
  String password = doc["password"] | "";
  ssid.trim();
  password.trim();
  
  if (ssid.length() == 0) {
    Serial.println("[BLE-COMMAND] Erreur: SSID vide");
    return false;
  }
  if (ssid.length() >= 64) {
    Serial.println("[BLE-COMMAND] Erreur: SSID trop long (max 63 caracteres)");
    return false;
  }
  if (password.length() >= 64) {
    Serial.println("[BLE-COMMAND] Erreur: Mot de passe trop long (max 63 caracteres)");
    return false;
  }
  
  #ifdef HAS_LED
  if (LEDManager::isInitialized()) {
    LEDManager::setEffect(LED_EFFECT_RAINBOW);
    Serial.println("[BLE-COMMAND] Effet RAINBOW active (sera arrete apres envoi de la reponse)");
  }
  #endif
  
  #ifdef HAS_WIFI
  if (!WiFiManager::isAvailable()) {
    Serial.println("[BLE-COMMAND] ERREUR: WiFi non disponible");
    return false;
  }
  
  if (WiFiManager::isRetryThreadActive()) {
    Serial.println("[BLE-COMMAND] Arret du thread de retry WiFi actif...");
    WiFiManager::stopRetryThread();
    delay(200);
  }
  
  if (WiFiManager::isConnected()) {
    Serial.println("[BLE-COMMAND] Deconnexion WiFi actuelle...");
    WiFiManager::disconnect();
    delay(500);
  }
  
  Serial.println("[BLE-COMMAND] Test de connexion avec les nouvelles credentials (tache dediee)...");
  Serial.print("[BLE-COMMAND]   SSID: ");
  Serial.println(ssid);
  Serial.print("[BLE-COMMAND]   Password: ");
  Serial.println(password.length() > 0 ? "********" : "(aucun)");
  
  SetupAsyncContext* ctx = new SetupAsyncContext();
  strncpy(ctx->ssid, ssid.c_str(), sizeof(ctx->ssid) - 1);
  ctx->ssid[sizeof(ctx->ssid) - 1] = '\0';
  strncpy(ctx->password, password.c_str(), sizeof(ctx->password) - 1);
  ctx->password[sizeof(ctx->password) - 1] = '\0';
  
  s_setupAsyncPending = true;
  WiFiManager::connectAsync(
    ctx->ssid,
    ctx->password[0] ? ctx->password : nullptr,
    20000,  // Timeout 20s (aligné avec WiFiManager)
    onSetupConnectComplete,
    ctx
  );
  return false;  // Handler verifiera isAsyncPending() et n'enverra pas de reponse
  #else
  Serial.println("[BLE-COMMAND] ERREUR: WiFi non disponible sur ce modele");
  return false;
  #endif
}
