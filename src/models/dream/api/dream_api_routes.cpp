/**
 * Routes API spécifiques au modèle Kidoo Dream
 */

#include "dream_api_routes.h"
#include "app_config.h"
#include "common/managers/api/api_manager.h"
#include "common/utils/mac_utils.h"

#ifdef HAS_WIFI

#include <esp_mac.h>

// Convertir MAC AA:BB:CC:DD:EE:FF -> AABBCCDDEEFF (sans séparateurs)
static void macToPathFormat(const char* macStr, char* out, size_t outSize) {
  for (size_t i = 0, j = 0; macStr[i] && j < outSize - 1; i++) {
    if (macStr[i] != ':' && macStr[i] != '-') {
      out[j++] = (macStr[i] >= 'a' && macStr[i] <= 'z') ? macStr[i] - 32 : macStr[i];
    }
  }
  out[outSize - 1] = '\0';
}

bool DreamApiRoutes::postNighttimeAlert() {
  char macStr[18];
  if (!getMacAddressString(macStr, sizeof(macStr), ESP_MAC_WIFI_STA)) {
    Serial.println("[DREAM] nighttime-alert: erreur MAC");
    return false;
  }

  char macClean[13] = {0};
  macToPathFormat(macStr, macClean, sizeof(macClean));

  char path[80];
  snprintf(path, sizeof(path), "/api/devices/%s/nighttime-alert", macClean);

  const int maxRetries = 3;
  const int timeoutMs = 8000;
  int code = -1;

  for (int attempt = 1; attempt <= maxRetries; attempt++) {
    code = ApiManager::getJsonWithDeviceAuth(path, timeoutMs);
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
