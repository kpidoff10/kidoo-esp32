#ifndef MQTT_ROUTES_H
#define MQTT_ROUTES_H

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * Interface pour les routes MQTT
 * 
 * Les routes permettent de traiter des messages JSON structurés
 * reçus via MQTT avec des actions spécifiques.
 * 
 * Format des messages attendus:
 * {
 *   "action": "nom_action",
 *   "params": { ... }
 * }
 */

class MQTTRoutes {
public:
  /**
   * Traiter un message JSON reçu via MQTT
   * @param json Le message JSON à traiter
   * @return true si le message a été traité, false sinon
   */
  static bool processMessage(const JsonObject& json);
  
  /**
   * Afficher les routes disponibles
   */
  static void printRoutes();
};

#endif // MQTT_ROUTES_H
