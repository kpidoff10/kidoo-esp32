#ifndef PUBNUB_ROUTES_H
#define PUBNUB_ROUTES_H

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * Interface pour les routes PubNub
 * 
 * Les routes permettent de traiter des messages JSON structurés
 * reçus via PubNub avec des actions spécifiques.
 * 
 * Format des messages attendus:
 * {
 *   "action": "nom_action",
 *   "params": { ... }
 * }
 */

class PubNubRoutes {
public:
  /**
   * Traiter un message JSON reçu via PubNub
   * @param json Le message JSON à traiter
   * @return true si le message a été traité, false sinon
   */
  static bool processMessage(const JsonObject& json);
  
  /**
   * Afficher les routes disponibles
   */
  static void printRoutes();
};

#endif // PUBNUB_ROUTES_H
