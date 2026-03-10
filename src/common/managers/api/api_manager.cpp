/**
 * Gestionnaire des requêtes HTTP vers l'API Kidoo
 * Méthodes génériques uniquement. La logique spécifique (ex: nighttime-alert)
 * reste dans les modèles (dream, gotchi, etc.).
 */

#include "api_manager.h"
#include "common/managers/wifi/wifi_manager.h"
#include "app_config.h"
#include "ssl_config.h"

#ifdef HAS_WIFI

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#if defined(HAS_SD) && defined(HAS_RTC)
#include "common/managers/device_key/device_key_manager.h"
#include "common/managers/rtc/rtc_manager.h"
#endif

// Configurer le client SSL avec la chaîne de certificats (protection contre MITM)
// Utilise le certificat intermédiaire R12 qui valide la chaîne complète
static void initSecureClient(WiFiClientSecure& client) {
  // Ajouter le certificat intermédiaire (signe le certificat du serveur)
  client.setCACert(LETS_ENCRYPT_R12);
}

int ApiManager::postJson(const char* path, const char* body, int timeoutMs) {
  if (!WiFiManager::isConnected()) {
    Serial.println("[API] WiFi non connecte, requete annulee");
    return -1;
  }

  char url[256];
  snprintf(url, sizeof(url), "%s%s", API_BASE_URL, path);
  WiFiClientSecure client;
  initSecureClient(client);  // Vérifier le certificat CA (protection MITM)
  HTTPClient http;
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(timeoutMs);
  int code = http.POST(body);
  http.end();
  return code;
}

int ApiManager::getJsonWithDeviceAuth(const char* path, String* responseBody, int timeoutMs) {
  if (!WiFiManager::isConnected()) {
    Serial.println("[API] WiFi non connecte, requete annulee");
    return -1;
  }

#if defined(HAS_SD) && defined(HAS_RTC)
  bool rtcAvailable = RTCManager::isAvailable();

  if (rtcAvailable) {
    // RTC disponible: faire requête AVEC signature
    uint32_t timestamp = RTCManager::getUnixTime();
    char timestampStr[12];
    snprintf(timestampStr, sizeof(timestampStr), "%lu", (unsigned long)timestamp);

    // Message: GET\nPATH\nTIMESTAMP (identique au serveur)
    char message[512];
    snprintf(message, sizeof(message), "GET\n%s\n%s", path, timestampStr);

    char signatureB64[96] = {0};
    if (!DeviceKeyManager::signMessageBase64((const uint8_t*)message, strlen(message), signatureB64, sizeof(signatureB64))) {
      Serial.println("[API] Erreur signature device");
      return -1;
    }

    char url[256];
    snprintf(url, sizeof(url), "%s%s", API_BASE_URL, path);
    WiFiClientSecure client;
    initSecureClient(client);  // Vérifier le certificat CA (protection MITM)
    HTTPClient http;
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("x-kidoo-timestamp", timestampStr);
    http.addHeader("x-kidoo-signature", signatureB64);
    http.setTimeout(timeoutMs);
    int code = http.GET();
    if (code > 0 && responseBody) {
      *responseBody = http.getString();
    }
    http.end();
    return code;
  } else {
    // RTC non disponible: faire requête SANS signature
    Serial.println("[API] RTC non disponible - requete sans signature");
  }
#endif

  // Fallback : GET simple sans signature (RTC non dispo ou HAS_SD/HAS_RTC non définis)
  char url[256];
  snprintf(url, sizeof(url), "%s%s", API_BASE_URL, path);
  WiFiClientSecure client;
  initSecureClient(client);  // Vérifier le certificat CA (protection MITM)
  HTTPClient http;
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(timeoutMs);
  int code = http.GET();
  if (code > 0 && responseBody) {
    *responseBody = http.getString();
  }
  http.end();
  return code;
}

int ApiManager::getJsonWithDeviceAuth(const char* path, int timeoutMs) {
  return getJsonWithDeviceAuth(path, nullptr, timeoutMs);
}

#endif // HAS_WIFI
