/**
 * Routes API spécifiques au modèle Kidoo Dream
 */

#include "dream_api_routes.h"
#include "app_config.h"
#include "common/managers/api/api_manager.h"
#include "common/utils/mac_utils.h"

#ifdef HAS_WIFI

#include <esp_mac.h>

bool DreamApiRoutes::postNighttimeAlert() {
  char macStr[18];
  if (!getMacAddressString(macStr, sizeof(macStr), ESP_MAC_WIFI_STA)) {
    Serial.println("[DREAM] nighttime-alert: erreur MAC");
    return false;
  }

  char body[64];
  snprintf(body, sizeof(body), "{\"mac\":\"%s\"}", macStr);

  const int maxRetries = 3;
  const int timeoutMs = 8000;
  int code = -1;

  for (int attempt = 1; attempt <= maxRetries; attempt++) {
    code = ApiManager::postJson("/api/device/nighttime-alert", body, timeoutMs);
    if (code == 200) {
      Serial.println("[DREAM] nighttime-alert -> OK");
      return true;
    }
    if (attempt < maxRetries) {
      Serial.printf("[DREAM] nighttime-alert tentative %d/%d: code=%d, retry...\n", attempt, maxRetries, code);
      delay(300);
    }
  }

  if (code > 0) {
    Serial.printf("[DREAM] nighttime-alert HTTP: %d\n", code);
  } else {
    Serial.printf("[DREAM] nighttime-alert echec apres %d tentatives: code=%d\n", maxRetries, code);
  }
  return false;
}

#endif // HAS_WIFI
