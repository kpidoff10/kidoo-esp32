#include "../managers/init/init_manager.h"
#include "../managers/wifi/wifi_manager.h"
#include "../managers/sd/sd_manager.h"
#include "../managers/rtc/rtc_manager.h"
#include "../../model_config.h"

bool InitManager::initWiFi() {
  systemStatus.wifi = INIT_IN_PROGRESS;
  
#ifndef HAS_WIFI
  systemStatus.wifi = INIT_NOT_STARTED;
  return false;
#else
  if (!HAS_WIFI) {
    systemStatus.wifi = INIT_NOT_STARTED;
    return false;
  }
  // Initialiser le WiFi
  if (!WiFiManager::init()) {
    systemStatus.wifi = INIT_FAILED;
    Serial.println("[INIT] ERREUR: Echec initialisation WiFi");
    return false;
  }
  
  if (!WiFiManager::isAvailable()) {
    systemStatus.wifi = INIT_FAILED;
    Serial.println("[INIT] WARNING: WiFi non disponible");
    return false;
  }
  
  // Tenter de se connecter au WiFi configuré
  const SDConfig& config = InitManager::getConfig();
  
  if (strlen(config.wifi_ssid) > 0) {
    Serial.print("[INIT] Tentative de connexion WiFi a: ");
    Serial.println(config.wifi_ssid);
    
    if (WiFiManager::connect()) {
      systemStatus.wifi = INIT_SUCCESS;
      Serial.println("[INIT] WiFi connecte");
      
      // Synchronisation RTC automatique après connexion WiFi
      #ifdef HAS_RTC
      RTCManager::autoSyncIfNeeded();
      #endif
      
      return true;
    } else {
      // Connexion échouée mais WiFi initialisé
      systemStatus.wifi = INIT_SUCCESS;  // WiFi est initialisé, juste pas connecté
      Serial.println("[INIT] WiFi initialise (non connecte - demarrage retry automatique)");
      
      // Démarrer le thread de retry automatique
      WiFiManager::startRetryThread();
      
      return true;
    }
  } else {
    // Pas de config WiFi, mais WiFi initialisé
    systemStatus.wifi = INIT_SUCCESS;
    Serial.println("[INIT] WiFi initialise (aucun SSID configure)");
    return true;
  }
#endif
}
