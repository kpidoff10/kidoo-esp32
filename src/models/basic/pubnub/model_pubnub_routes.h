#ifndef MODEL_BASIC_PUBNUB_ROUTES_H
#define MODEL_BASIC_PUBNUB_ROUTES_H

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * Routes PubNub spécifiques au modèle Kidoo Basic
 * 
 * Actions disponibles:
 * - get-info: Récupérer les informations complètes de l'appareil
 * - brightness: Gérer la luminosité des LEDs
 * - sleep-timeout: Gérer le délai de mise en veille
 * - reboot: Redémarrer l'appareil
 * - led: Contrôler les LEDs (couleur, effet)
 * 
 * Format des messages:
 * { "action": "get-info" }
 * { "action": "brightness", "params": { "value": 50 } }
 * { "action": "sleep-timeout", "params": { "value": 30000 } }
 * { "action": "reboot", "params": { "delay": 1000 } }
 * { "action": "led", "color": "#FF0000", "effect": "solid" }
 */

class ModelBasicPubNubRoutes {
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
  // Handlers pour chaque action
  static bool handleGetInfo(const JsonObject& json);
  static bool handleBrightness(const JsonObject& json);
  static bool handleSleepTimeout(const JsonObject& json);
  static bool handleReboot(const JsonObject& json);
  static bool handleLed(const JsonObject& json);
};

#endif // MODEL_BASIC_PUBNUB_ROUTES_H
