#ifndef MODEL_GOTCHI_MQTT_ROUTES_H
#define MODEL_GOTCHI_MQTT_ROUTES_H

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * Routes MQTT — Gotchi (Waveshare ESP32-S3 AMOLED)
 */

class ModelGotchiMqttRoutes {
public:
  static bool processMessage(const JsonObject& actionObj);
  static void printRoutes();
};

#endif // MODEL_GOTCHI_MQTT_ROUTES_H
