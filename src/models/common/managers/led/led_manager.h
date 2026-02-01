#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "../../../model_config.h"
#include "../../config/core_config.h"

/**
 * Gestionnaire de LEDs dans un thread séparé (Core 1)
 * 
 * Ce module garantit que la gestion des LEDs ne s'arrête jamais,
 * même si d'autres parties du code buggent ou se bloquent.
 * 
 * Architecture :
 * - Tourne sur Core 1 (CORE_LED) pour éviter les conflits avec WiFi sur Core 0
 * - Utilise la PSRAM pour le buffer LED (si USE_PSRAM_FOR_LED_BUFFER = true)
 * - Priorité élevée (PRIORITY_LED) pour des animations fluides
 */

// Types de commandes pour le thread LED
enum LEDCommandType {
  LED_CMD_SET_COLOR,        // Définir une couleur RGB
  LED_CMD_SET_BRIGHTNESS,   // Changer la luminosité
  LED_CMD_SET_EFFECT,       // Activer un effet
  LED_CMD_CLEAR,            // Éteindre toutes les LEDs
  LED_CMD_TEST_SEQUENTIAL   // Test séquentiel des LEDs
};

// Types d'effets disponibles
enum LEDEffect {
  LED_EFFECT_NONE,          // Pas d'effet (couleur unie)
  LED_EFFECT_RAINBOW,       // Arc-en-ciel
  LED_EFFECT_PULSE,         // Pulsation
  LED_EFFECT_GLOSSY,        // Effet glossy
  LED_EFFECT_ROTATE,        // Effet de rotation (utilise la couleur définie)
  LED_EFFECT_NIGHTLIGHT,    // Effet de veilleuse (vagues bleu/blanc)
  LED_EFFECT_BREATHE,       // Effet de respiration avec changement de couleur
  LED_EFFECT_RAINBOW_SOFT   // Arc-en-ciel doux (animation lente pour veilleuse)
};

// Structure de commande pour le thread LED
struct LEDCommand {
  LEDCommandType type;
  union {
    struct {
      uint8_t r;
      uint8_t g;
      uint8_t b;
    } color;
    uint8_t brightness;
    LEDEffect effect;
  } data;
};

class LEDManager {
public:
  // Initialiser le gestionnaire LED (créer le thread)
  static bool init();
  
  // Arrêter le gestionnaire (ne devrait jamais être appelé)
  static void stop();
  
  // Envoyer une commande au thread LED
  static bool sendCommand(const LEDCommand& cmd);
  
  // Méthodes pratiques pour envoyer des commandes
  static bool setColor(uint8_t r, uint8_t g, uint8_t b);
  static bool setBrightness(uint8_t brightness);
  static bool setEffect(LEDEffect effect);
  static bool clear();
  
  // Gestion du sleep mode
  static void wakeUp();  // Réveiller les LEDs (reset du timer d'inactivité)
  static bool getSleepState();  // Vérifier si les LEDs sont en mode sleep
  static void preventSleep();  // Empêcher le sleep mode (pour bedtime, etc.)
  static void allowSleep();  // Réautoriser le sleep mode
  
  // Obtenir l'état actuel
  static bool isInitialized();
  static uint8_t getCurrentBrightness();
  
  // Test des LEDs une par une
  static bool testLEDsSequential();  // Test séquentiel : allume chaque LED une par une puis toutes en rouge

private:
  // Thread principal de gestion des LEDs
  static void ledTask(void* parameter);
  
  // Traiter une commande reçue
  static void processCommand(const LEDCommand& cmd);
  
  // Appliquer les effets animés
  static void updateEffects();
  
  // Gestion du sleep mode
  static void checkSleepMode();
  static void updateSleepFade();  // Animation de fade vers sleep
  static void updateWakeFade();  // Animation de fade depuis sleep
  static void resetPulseEffect();  // Réinitialiser l'effet PULSE pour transition fluide
  
  // Utilitaire pour obtenir le nom d'un effet
  static const char* getEffectName(LEDEffect effect);
  
  // Variables statiques
  static bool initialized;
  static TaskHandle_t taskHandle;
  static QueueHandle_t commandQueue;
  static Adafruit_NeoPixel* strip;
  static uint8_t currentBrightness;
  static LEDEffect currentEffect;
  static uint32_t currentColor;  // Couleur au format RGB (0xRRGGBB)
  static unsigned long lastUpdateTime;
  static unsigned long lastActivityTime;  // Dernière activité (pour sleep mode)
  static unsigned long rotateActivationTime;  // Temps d'activation de ROTATE pour désactivation auto
  static bool isSleeping;  // État du sleep mode
  static bool isFadingToSleep;  // En cours d'animation de fade vers sleep
  static bool isFadingFromSleep;  // En cours d'animation de fade depuis sleep
  static unsigned long sleepFadeStartTime;  // Début de l'animation de fade
  static LEDEffect savedEffect;  // Effet sauvegardé avant le sleep
  static uint32_t sleepTimeoutMs;  // Timeout configuré pour le sleep mode
  static bool sleepPrevented;  // Flag pour empêcher le sleep mode (bedtime, etc.)
  static bool pulseNeedsReset;  // Flag pour réinitialiser l'effet PULSE
  static bool hardwareInitialized;  // Init NeoPixel faite dans la tâche LED
  
  // Variables pour le test séquentiel
  static bool testSequentialActive;  // Test séquentiel en cours
  static int testSequentialIndex;  // Index de la LED actuelle dans le test
  static unsigned long testSequentialLastUpdate;  // Dernière mise à jour du test

  // Paramètres du thread (centralisés dans core_config.h)
  static const int QUEUE_SIZE = 10;
  static const int TASK_STACK_SIZE = STACK_SIZE_LED;
  static const int TASK_PRIORITY = PRIORITY_LED;
  static const int TASK_CORE = CORE_LED;  // Core 1 pour temps-réel
  static const int UPDATE_INTERVAL_MS = 16;  // ~60 FPS pour les animations
};

#endif // LED_MANAGER_H
