#ifndef MODEL_SOUND_MQTT_ROUTES_H
#define MODEL_SOUND_MQTT_ROUTES_H

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * Routes MQTT pour Sound (Boîte à musique)
 * Traite les messages MQTT spécifiques au modèle Sound
 */

class ModelSoundMqttRoutes {
public:
  /**
   * Traite un message d'action reçu via MQTT
   * @param actionObj Objet JSON de l'action
   * @return true si le message a été traité, false sinon
   */
  static bool processMessage(const JsonObject& actionObj);

  /**
   * Affiche les routes disponibles
   */
  static void printRoutes();
};

#endif // MODEL_SOUND_MQTT_ROUTES_H
