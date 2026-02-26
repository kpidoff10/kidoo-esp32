/**
 * Utilitaires pour les adresses MAC
 * Fonctions pour obtenir et formater les adresses MAC WiFi
 */

#ifndef MAC_UTILS_H
#define MAC_UTILS_H

#include <Arduino.h>

#ifdef HAS_WIFI
#include <WiFi.h>
#include <esp_mac.h>

/**
 * Obtenir l'adresse MAC WiFi et la formater en string
 * 
 * @param macStr Buffer de sortie pour l'adresse MAC formatée (doit être au moins de 18 caractères)
 * @param macStrSize Taille du buffer (doit être >= 18)
 * @param macType Type de MAC à récupérer (ESP_MAC_WIFI_STA par défaut)
 * @return true si l'adresse MAC a été obtenue et formatée avec succès, false sinon
 */
bool getMacAddressString(char* macStr, size_t macStrSize, esp_mac_type_t macType = ESP_MAC_WIFI_STA);

/**
 * Obtenir l'adresse MAC WiFi et la retourner comme String
 * 
 * @param macType Type de MAC à récupérer (ESP_MAC_WIFI_STA par défaut)
 * @return String contenant l'adresse MAC formatée (format: AA:BB:CC:DD:EE:FF)
 */
String getMacAddressString(esp_mac_type_t macType = ESP_MAC_WIFI_STA);

#else
// Stubs pour les modèles sans WiFi
inline bool getMacAddressString(char* macStr, size_t macStrSize, int macType = 0) {
  (void)macStr;
  (void)macStrSize;
  (void)macType;
  return false;
}

inline String getMacAddressString(int macType = 0) {
  (void)macType;
  return String("");
}
#endif

#endif // MAC_UTILS_H
