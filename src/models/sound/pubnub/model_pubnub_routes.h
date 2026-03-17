#ifndef MODEL_SOUND_PUBNUB_ROUTES_H
#define MODEL_SOUND_PUBNUB_ROUTES_H

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * Routes PubNub pour Sound (Boîte à musique)
 * Traite les messages PubNub spécifiques au modèle Sound
 */

class ModelSoundPubNubRoutes {
public:
  /**
   * Traite un message d'action reçu via PubNub
   * @param actionObj Objet JSON de l'action
   * @return true si le message a été traité, false sinon
   */
  static bool processMessage(const JsonObject& actionObj);

  /**
   * Affiche les routes disponibles
   */
  static void printRoutes();
};

#endif // MODEL_SOUND_PUBNUB_ROUTES_H
