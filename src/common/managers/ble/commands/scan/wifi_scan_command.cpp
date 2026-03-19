#include "wifi_scan_command.h"
#include <ArduinoJson.h>
#include "../ble_command_handler.h"

#ifdef HAS_WIFI
#include <WiFi.h>
#include "common/managers/wifi/wifi_manager.h"
#endif

bool BLEWiFiScanCommand::isValid(const String& jsonData) {
  if (jsonData.length() == 0) return false;

  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  StaticJsonDocument<256> doc;
  #pragma GCC diagnostic pop
  DeserializationError error = deserializeJson(doc, jsonData);
  if (error) return false;
  if (!doc["command"].is<String>() || doc["command"] != "wifi-scan") return false;
  return true;
}

bool BLEWiFiScanCommand::execute(const String& jsonData) {
#ifndef HAS_WIFI
  BLECommandHandler::sendResponse(false, "WiFi non disponible");
  return false;
#else
  int numNetworks = WiFi.scanNetworks(false, false);

  if (numNetworks < 0) {
    Serial.printf("[WIFI-SCAN] ERREUR: scanNetworks retourné %d\n", numNetworks);
    BLECommandHandler::sendResponse(false, "Echec du scan WiFi");
    return false;
  }

  // Dédupliquer par SSID en gardant le meilleur RSSI
  struct Network { String ssid; int rssi; uint8_t security; };
  Network best[16];
  int bestCount = 0;

  for (int i = 0; i < numNetworks && bestCount < 16; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) continue;

    int existing = -1;
    for (int j = 0; j < bestCount; j++) {
      if (best[j].ssid == ssid) { existing = j; break; }
    }

    int rssi = WiFi.RSSI(i);
    if (existing >= 0) {
      if (rssi > best[existing].rssi) best[existing].rssi = rssi;
    } else {
      best[bestCount++] = { ssid, rssi, (uint8_t)WiFi.encryptionType(i) };
    }
  }

  // Construire la réponse — limiter à 10
  JsonDocument doc;
  doc["success"] = true;
  JsonArray networks = doc["networks"].to<JsonArray>();

  int maxNetworks = min(bestCount, 10);
  for (int i = 0; i < maxNetworks; i++) {
    JsonObject net = networks.add<JsonObject>();
    net["ssid"] = best[i].ssid;
    net["rssi"] = best[i].rssi;
    net["security"] = best[i].security;
  }

  WiFi.scanDelete();

  String responseJson;
  serializeJson(doc, responseJson);
  BLECommandHandler::sendRawJson(responseJson);
  return true;
#endif
}
