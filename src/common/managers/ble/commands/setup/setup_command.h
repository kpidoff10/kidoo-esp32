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
   * Exécuter la commande setup (non bloquant - connexion WiFi en tâche dédiée)
   * @param jsonData Données JSON reçues
   * @return true si succès synchrone, false si échec ou async en cours (callback enverra la réponse)
   */
  static bool execute(const String& jsonData);
  
  /**
   * Vérifier si une réponse async est en attente (ne pas envoyer de réponse dans le handler)
   */
  static bool isAsyncPending();
  
  /**
   * Vérifier si la commande est valide
   * @param jsonData Données JSON reçues
   * @return true si la commande est valide, false sinon
   */
  static bool isValid(const String& jsonData);
};

#endif // BLE_SETUP_COMMAND_H
