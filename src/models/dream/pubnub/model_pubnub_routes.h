#ifndef MODEL_DREAM_PUBNUB_ROUTES_H
#define MODEL_DREAM_PUBNUB_ROUTES_H

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * Routes PubNub spécifiques au modèle Kidoo Dream
 * 
 * Actions disponibles:
 * - get-info: Récupérer les informations complètes de l'appareil
 * - brightness: Gérer la luminosité des LEDs
 * - sleep-timeout: Gérer le délai de mise en veille
 * - reboot: Redémarrer l'appareil
 * - led: Contrôler les LEDs (couleur, effet)
 * - start-test-bedtime: Démarrer le test de l'heure de coucher
 * - stop-test-bedtime: Arrêter le test de l'heure de coucher
 * - start-bedtime: Démarrer manuellement la routine de coucher (empêche le déclenchement automatique)
 * - stop-bedtime: Arrêter manuellement la routine de coucher
 * - stop-routine: Arrêter la routine active (bedtime ou wakeup)
 * - set-bedtime-config: Sauvegarder la configuration de l'heure de coucher sur la SD
 * - start-test-wakeup: Démarrer le test de l'heure de réveil
 * - stop-test-wakeup: Arrêter le test de l'heure de réveil
 * - set-wakeup-config: Sauvegarder la configuration de l'heure de réveil sur la SD
 * 
 * Format des messages:
 * { "action": "get-info" }
 * { "action": "brightness", "params": { "value": 50 } }
 * { "action": "sleep-timeout", "params": { "value": 30000 } }
 * { "action": "reboot", "params": { "delay": 1000 } }
 * { "action": "led", "color": "#FF0000", "effect": "solid" }
 * { "action": "start-test-bedtime", "params": { "colorR": 255, "colorG": 107, "colorB": 107, "brightness": 50 } }
 * { "action": "stop-test-bedtime" }
 * { "action": "start-bedtime" }
 * { "action": "stop-bedtime" }
 * { "action": "set-bedtime-config", "params": { "colorR": 255, "colorG": 107, "colorB": 107, "brightness": 50, "allNight": false, "weekdaySchedule": {...} } }
 * { "action": "start-test-wakeup", "params": { "colorR": 255, "colorG": 200, "colorB": 100, "brightness": 50 } }
 * { "action": "stop-test-wakeup" }
 * { "action": "set-wakeup-config", "params": { "colorR": 255, "colorG": 200, "colorB": 100, "brightness": 50, "weekdaySchedule": {...} } }
 */

class ModelDreamPubNubRoutes {
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
  
  /**
   * Vérifier le timeout du test de bedtime (à appeler périodiquement)
   */
  static void checkTestBedtimeTimeout();
  
  /**
   * Vérifier si le test de bedtime est actif
   */
  static bool isTestBedtimeActive();
  
  /**
   * Vérifier le timeout du test de wakeup (à appeler périodiquement)
   */
  static void checkTestWakeupTimeout();
  
  /**
   * Vérifier si le test de wakeup est actif
   */
  static bool isTestWakeupActive();

private:
  // Handlers pour chaque action
  static bool handleGetInfo(const JsonObject& json);
  static bool handleBrightness(const JsonObject& json);
  static bool handleSleepTimeout(const JsonObject& json);
  static bool handleReboot(const JsonObject& json);
  static bool handleLed(const JsonObject& json);
  static bool handleStartTestBedtime(const JsonObject& json);
  static bool handleStopTestBedtime(const JsonObject& json);
  static bool handleStartBedtime(const JsonObject& json);
  static bool handleStopBedtime(const JsonObject& json);
  static bool handleStopRoutine(const JsonObject& json);
  static bool handleSetBedtimeConfig(const JsonObject& json);
  static bool handleStartTestWakeup(const JsonObject& json);
  static bool handleStopTestWakeup(const JsonObject& json);
  static bool handleSetWakeupConfig(const JsonObject& json);
};

#endif // MODEL_DREAM_PUBNUB_ROUTES_H
