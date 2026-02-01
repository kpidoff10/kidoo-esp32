#include "mac_utils.h"

#ifdef HAS_WIFI
#include <WiFi.h>
#include <esp_mac.h>

bool getMacAddressString(char* macStr, size_t macStrSize, esp_mac_type_t macType) {
  if (macStr == nullptr || macStrSize < 18) {
    return false;
  }

  // Obtenir l'adresse MAC
  uint8_t mac[6];
  esp_err_t err = esp_read_mac(mac, macType);
  if (err != ESP_OK) {
    // Fallback: utiliser WiFi.macAddress()
    WiFi.macAddress(mac);
  }

  // Formater l'adresse MAC en string (format: AA:BB:CC:DD:EE:FF)
  snprintf(macStr, macStrSize, "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  return true;
}

String getMacAddressString(esp_mac_type_t macType) {
  char macStr[18];
  if (getMacAddressString(macStr, sizeof(macStr), macType)) {
    return String(macStr);
  }
  return String("");
}
#endif
