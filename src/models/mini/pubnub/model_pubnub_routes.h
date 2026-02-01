#ifndef MODEL_MINI_PUBNUB_ROUTES_H
#define MODEL_MINI_PUBNUB_ROUTES_H

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * Routes PubNub spécifiques au modèle Kidoo Mini
 * 
 * Actions disponibles:
 * - brightness: Gérer la luminosité des LEDs
 * - sleep: Gérer le mode veille
 * - led: Contrôler les LEDs (couleur, effet)
 * - status: Demander le statut de l'appareil
 */

class ModelMiniPubNubRoutes {
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

private:
  static bool handleBrightness(const JsonObject& json);
  static bool handleSleep(const JsonObject& json);
  static bool handleLed(const JsonObject& json);
  static bool handleStatus(const JsonObject& json);
};

#endif // MODEL_MINI_PUBNUB_ROUTES_H
