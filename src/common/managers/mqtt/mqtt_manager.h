#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include "../../config/core_config.h"

/**
 * Gestionnaire MQTT (Thread séparé sur Core 0)
 *
 * Ce module gère la connexion MQTT au broker Mosquitto pour recevoir
 * des commandes à distance et publier des réponses.
 * Tourne dans un thread FreeRTOS séparé pour ne pas bloquer le loop principal.
 *
 * Architecture :
 * - Tourne sur Core 0 (CORE_MQTT) avec le WiFi stack
 * - Priorité basse (PRIORITY_MQTT) car non critique en temps-réel
 * - Partage Core 0 avec WiFi pour minimiser les context switches réseau
 * - Utilise PubSubClient pour la connexion MQTT persistante
 */

class MqttManager {
public:
  /**
   * Initialiser le client MQTT et démarrer le thread
   * @return true si l'initialisation réussit
   */
  static bool init();

  /**
   * Se connecter au broker MQTT (subscribe aux topics)
   * @return true si la connexion réussit
   */
  static bool connect();

  /**
   * Se déconnecter du broker MQTT
   */
  static void disconnect();

  /**
   * Arrêt complet pour OTA : supprime la task et libère la queue.
   * Réinitialise l'état (initialized=false). Appeler init() pour restaurer.
   */
  static void shutdownForOta();

  /**
   * Vérifier si le client est connecté
   * @return true si connecté
   */
  static bool isConnected();

  /**
   * Vérifier si le client est initialisé
   * @return true si initialisé
   */
  static bool isInitialized();

  /**
   * Vérifier si MQTT est disponible (WiFi connecté + config valide)
   * @return true si disponible
   */
  static bool isAvailable();

  /**
   * Boucle de maintenance MQTT (appelée automatiquement par le thread)
   * Ne fait rien si appelée manuellement - le thread gère tout
   */
  static void loop();

  /**
   * Publier un message sur le topic telemetry (thread-safe)
   * @param message Le message à publier
   * @return true si la publication réussit
   */
  static bool publish(const char* message);

  /**
   * Publier le statut du device
   * @return true si la publication réussit
   */
  static bool publishStatus();

  /**
   * Afficher les informations MQTT
   */
  static void printInfo();

  /**
   * Obtenir le topic de commande
   * @return Le nom du topic cmd
   */
  static const char* getCmdTopic();

  /**
   * Obtenir le topic de télémétrie
   * @return Le nom du topic telemetry
   */
  static const char* getTelemetryTopic();

private:
  // Fonction du thread FreeRTOS
  static void threadFunction(void* parameter);

  // Callback pour les messages MQTT reçus
  static void onMessage(char* topic, byte* payload, unsigned int length);

  // Traiter les messages reçus
  static void processMessage(const JsonObject& json);

  // Publication interne (appelée depuis le thread)
  static bool publishInternal(const char* message);

  // Variables statiques
  static bool initialized;
  static bool connected;
  static bool threadRunning;
  static char cmdTopic[80];
  static char telemetryTopic[80];
  static char clientId[64];
  static WiFiClientSecure espClient;
  static PubSubClient mqttClient;
  static TaskHandle_t taskHandle;

  // File d'attente pour les messages à publier
  static QueueHandle_t publishQueue;

  // Taille de la file de publication
  static constexpr int PUBLISH_QUEUE_SIZE = 5;
};

#endif // MQTT_MANAGER_H
