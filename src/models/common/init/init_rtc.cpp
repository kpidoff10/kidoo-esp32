#include "../managers/init/init_manager.h"
#include "../managers/rtc/rtc_manager.h"
#include "../../model_config.h"

bool InitManager::initRTC() {
#ifndef HAS_RTC
  systemStatus.rtc = INIT_NOT_STARTED;
  return true;
#else
  if (!HAS_RTC) {
    systemStatus.rtc = INIT_NOT_STARTED;
    return true;
  }
  systemStatus.rtc = INIT_IN_PROGRESS;
  Serial.println("[INIT] Initialisation RTC DS3231...");
  
  if (!RTCManager::init()) {
    Serial.println("[INIT] ERREUR: RTC non disponible");
    systemStatus.rtc = INIT_FAILED;
    return false;
  }
  
  if (!RTCManager::isAvailable()) {
    Serial.println("[INIT] WARNING: RTC non detecte");
    systemStatus.rtc = INIT_FAILED;
    return false;
  }
  
  // Afficher l'état de l'heure
  if (RTCManager::hasLostPower()) {
    Serial.println("[INIT] RTC: Oscillateur arrete, sync NTP necessaire");
  } else if (!RTCManager::isTimeValid()) {
    Serial.println("[INIT] RTC: Heure invalide, sync NTP necessaire");
  }
  
  // Afficher l'heure actuelle
  Serial.print("[INIT] RTC: ");
  Serial.println(RTCManager::getDateTimeString());
  
  // Note: La synchronisation NTP sera faite automatiquement
  // après la connexion WiFi via autoSyncIfNeeded()
  
  systemStatus.rtc = INIT_SUCCESS;
  Serial.println("[INIT] RTC operationnel");
  
  return true;
#endif
}
