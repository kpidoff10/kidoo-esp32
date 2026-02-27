/**
 * Gestionnaire des requêtes HTTP vers l'API Kidoo
 * Méthodes génériques uniquement. La logique spécifique (ex: nighttime-alert)
 * reste dans les modèles (dream, gotchi, etc.).
 */

#include "api_manager.h"
#include "common/managers/wifi/wifi_manager.h"
#include "app_config.h"

#ifdef HAS_WIFI

#include <HTTPClient.h>
#include <WiFi.h>

int ApiManager::postJson(const char* path, const char* body, int timeoutMs) {
  if (!WiFiManager::isConnected()) {
    Serial.println("[API] WiFi non connecte, requete annulee");
    return -1;
  }

  char url[256];
  snprintf(url, sizeof(url), "%s%s", API_BASE_URL, path);
  WiFiClient client;
  HTTPClient http;
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  http.setConnectTimeout(5000);
  http.setTimeout(timeoutMs);
  int code = http.POST(body);
  http.end();
  client.stop();
  return code;
}

#endif // HAS_WIFI
