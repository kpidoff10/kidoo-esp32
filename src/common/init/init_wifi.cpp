#include "common/managers/init/init_manager.h"
#include "common/managers/wifi/wifi_manager.h"
#include "common/managers/sd/sd_manager.h"
#include "common/managers/rtc/rtc_manager.h"
#include "models/model_config.h"
#include "common/config/core_config.h"

#ifdef HAS_WIFI
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

#ifdef HAS_WIFI
/** Tâche WiFi boot : connexion en arrière-plan pour ne pas bloquer les LEDs */
static void wifiBootConnectTask(void* param) {
  const SDConfig& config = InitManager::getConfig();
  // Attendre que le hotspot soit prêt (5s pour hotspots S24)
  vTaskDelay(pdMS_TO_TICKS(5000));
  bool ok = WiFiManager::connect(config.wifi_ssid, config.wifi_password, 20000);
  if (ok) {
    #ifdef HAS_RTC
    RTCManager::autoSyncIfNeeded();
    #endif
  } else {
    WiFiManager::startRetryThread();
  }
  vTaskDelete(nullptr);
}
#endif

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
  
  const SDConfig& config = InitManager::getConfig();
  
  if (strlen(config.wifi_ssid) > 0) {
    Serial.print("[INIT] Tentative de connexion WiFi a: ");
    Serial.println(config.wifi_ssid);
    Serial.println("[INIT] Connexion en tache de fond (LEDs non bloquees)");

    // Lancer la connexion dans une tâche dédiée (priorité basse = LEDs restent fluides)
    xTaskCreatePinnedToCore(
      wifiBootConnectTask,
      "WiFiBoot",
      STACK_SIZE_WIFI_CONNECT,
      nullptr,
      PRIORITY_WIFI_RETRY,  // Priorité basse : LEDs (3) > WiFi connect (1)
      nullptr,
      CORE_WIFI_RETRY
    );
    
    return true;  // Retour immédiat, init_manager fera l'attente 8s
  } else {
    systemStatus.wifi = INIT_SUCCESS;
    Serial.println("[INIT] WiFi initialise (aucun SSID configure)");
    return true;
  }
#endif
}
