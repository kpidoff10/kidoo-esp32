#ifndef BLE_SETUP_COMMAND_H
#define BLE_SETUP_COMMAND_H

#include <Arduino.h>

/**
 * Commande BLE "setup"
 * Configure le WiFi avec SSID et password
 * 
 * Format JSON attendu:
 * {
 *   "command": "setup",
 *   "ssid": "MonReseauWiFi",
 *   "password": "MonMotDePasse"
 * }
 */

class BLESetupCommand {
public:
  /**
   * Exécuter la commande setup
   * @param jsonData Données JSON reçues
   * @return true si la commande a réussi, false sinon
   */
  static bool execute(const String& jsonData);
  
  /**
   * Vérifier si la commande est valide
   * @param jsonData Données JSON reçues
   * @return true si la commande est valide, false sinon
   */
  static bool isValid(const String& jsonData);
};

#endif // BLE_SETUP_COMMAND_H
