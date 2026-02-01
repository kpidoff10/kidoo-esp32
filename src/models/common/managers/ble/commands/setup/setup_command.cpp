#include "setup_command.h"
#include <ArduinoJson.h>
#include "../../../init/init_manager.h"
#include "../../../sd/sd_manager.h"
#include "../../../wifi/wifi_manager.h"
#include "../../../led/led_manager.h"

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

bool BLESetupCommand::execute(const String& jsonData) {
  Serial.println("[BLE-COMMAND] Execution de la commande 'setup'");
  
  // Parser le JSON
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
  
  // Extraire SSID et password
  String ssid = doc["ssid"] | "";
  String password = doc["password"] | "";
  
  ssid.trim();
  password.trim();
  
  // Valider le SSID
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
  
  // IMPORTANT: Tester la connexion WiFi AVANT de sauvegarder la configuration
  // On ne sauvegarde que si la connexion réussit
  
  // Activer l'effet RAINBOW pour indiquer la réception de la commande
  // L'effet sera arrêté dans ble_command_handler.cpp après l'envoi de la réponse
  #ifdef HAS_LED
  if (LEDManager::isInitialized()) {
    LEDManager::setEffect(LED_EFFECT_RAINBOW);
    Serial.println("[BLE-COMMAND] Effet RAINBOW active (sera arrete apres envoi de la reponse)");
  }
  #endif
  
  // Tenter de se connecter au WiFi AVANT de sauvegarder
  #ifdef HAS_WIFI
  bool wifiConnected = false;
  if (WiFiManager::isAvailable()) {
    Serial.println("[BLE-COMMAND] Test de connexion WiFi AVANT sauvegarde...");
    
    // Arrêter le thread de retry si actif (important avant de tester la connexion)
    if (WiFiManager::isRetryThreadActive()) {
      Serial.println("[BLE-COMMAND] Arret du thread de retry WiFi actif...");
      WiFiManager::stopRetryThread();
      delay(200); // Laisser le temps au thread de s'arrêter proprement
    }
    
    // Déconnecter si déjà connecté
    if (WiFiManager::isConnected()) {
      Serial.println("[BLE-COMMAND] Deconnexion WiFi actuelle...");
      WiFiManager::disconnect();
      delay(500);
    }
    
    // Tester la connexion avec les nouvelles credentials (sans sauvegarder)
    // On utilise connect() avec les paramètres directement
    Serial.println("[BLE-COMMAND] Test de connexion avec les nouvelles credentials...");
    Serial.print("[BLE-COMMAND]   SSID: ");
    Serial.println(ssid);
    Serial.print("[BLE-COMMAND]   Password: ");
    if (password.length() > 0) {
      Serial.println("********");
    } else {
      Serial.println("(aucun)");
    }
    
    // Tester la connexion directement avec les credentials fournis
    wifiConnected = WiFiManager::connect(ssid.c_str(), password.length() > 0 ? password.c_str() : nullptr);
    
    if (wifiConnected) {
      // Connexion réussie : sauvegarder la configuration
      Serial.println("[BLE-COMMAND] Connexion WiFi reussie! Sauvegarde de la configuration...");
      
      // Récupérer la configuration actuelle
      SDConfig config = InitManager::getConfig();
      
      // Mettre à jour le SSID et le password
      strncpy(config.wifi_ssid, ssid.c_str(), sizeof(config.wifi_ssid) - 1);
      config.wifi_ssid[sizeof(config.wifi_ssid) - 1] = '\0';
      strncpy(config.wifi_password, password.c_str(), sizeof(config.wifi_password) - 1);
      config.wifi_password[sizeof(config.wifi_password) - 1] = '\0';
      
      // Sauvegarder la configuration
      if (!SDManager::isAvailable()) {
        Serial.println("[BLE-COMMAND] ERREUR: Carte SD non disponible");
        WiFiManager::disconnect(); // Déconnecter car on ne peut pas sauvegarder
        wifiConnected = false;
      } else if (!InitManager::updateConfig(config)) {
        Serial.println("[BLE-COMMAND] ERREUR: Impossible de sauvegarder la configuration");
        WiFiManager::disconnect(); // Déconnecter car on ne peut pas sauvegarder
        wifiConnected = false;
      } else {
        Serial.println("[BLE-COMMAND] Configuration WiFi sauvegardee avec succes!");
      }
      
      // Note: Les LEDs seront gérées dans ble_command_handler.cpp après l'envoi de la réponse
      // On garde RAINBOW actif jusqu'à ce que la réponse soit envoyée
    } else {
      // Connexion échouée : ne PAS sauvegarder
      Serial.println("[BLE-COMMAND] Echec de connexion WiFi - Configuration NON sauvegardee");
      
      // Gérer les LEDs pour l'échec (effet rouge sera appliqué dans ble_command_handler.cpp)
      // On garde RAINBOW actif jusqu'à ce que la réponse soit envoyée
    }
  } else {
    Serial.println("[BLE-COMMAND] ERREUR: WiFi non disponible");
    wifiConnected = false;
  }
  #else
  // Pas de WiFi disponible
  Serial.println("[BLE-COMMAND] ERREUR: WiFi non disponible sur ce modele");
  wifiConnected = false;
  #endif
  
  // Retourner true seulement si la connexion WiFi a réussi ET si la configuration a été sauvegardée
  return wifiConnected;
}
