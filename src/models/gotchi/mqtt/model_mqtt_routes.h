#ifndef MODEL_GOTCHI_MQTT_ROUTES_H
#define MODEL_GOTCHI_MQTT_ROUTES_H

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * Routes MQTT spécifiques au modèle Kidoo Gotchi
 *
 * Actions: brightness, sleep, led, status, firmware-update
 */

class ModelGotchiMqttRoutes {
public:
  static bool processMessage(const JsonObject& json);
  static void printRoutes();

private:
  static bool handleBrightness(const JsonObject& json);
  static bool handleSleep(const JsonObject& json);
  static bool handleLed(const JsonObject& json);
  static bool handleStatus(const JsonObject& json);
  static bool handleFirmwareUpdate(const JsonObject& json);
};

#endif // MODEL_GOTCHI_MQTT_ROUTES_H
