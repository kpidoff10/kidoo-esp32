#include "led_manager.h"
#include "../init/init_manager.h"
#include "../sd/sd_manager.h"
#include "../../../model_config.h"
#include "../../config/core_config.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef HAS_WIFI
#include "../wifi/wifi_manager.h"
#endif

#ifdef HAS_BLE
#include "../ble_config/ble_config_manager.h"
#endif

// Fonction utilitaire pour convertir HSV en RGB (format NeoPixel)
static uint32_t hsvToRgb(uint8_t h, uint8_t s, uint8_t v) {
  uint8_t r, g, b;
  
  if (s == 0) {
    r = g = b = v;
  } else {
    uint8_t region = h / 43;
    uint8_t remainder = (h - (region * 43)) * 6;
    
    uint8_t p = (v * (255 - s)) >> 8;
    uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;
    
    switch (region) {
      case 0: r = v; g = t; b = p; break;
      case 1: r = q; g = v; b = p; break;
      case 2: r = p; g = v; b = t; break;
      case 3: r = p; g = q; b = v; break;
      case 4: r = t; g = p; b = v; break;
      default: r = v; g = p; b = q; break;
    }
  }
  
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

// Variables statiques
bool LEDManager::initialized = false;
TaskHandle_t LEDManager::taskHandle = nullptr;
QueueHandle_t LEDManager::commandQueue = nullptr;
Adafruit_NeoPixel* LEDManager::strip = nullptr;
uint8_t LEDManager::currentBrightness = DEFAULT_LED_BRIGHTNESS;
LEDEffect LEDManager::currentEffect = LED_EFFECT_NONE;
uint32_t LEDManager::currentColor = 0;  // Noir par défaut
unsigned long LEDManager::lastUpdateTime = 0;
unsigned long LEDManager::lastActivityTime = 0;
bool LEDManager::isSleeping = false;
bool LEDManager::isFadingToSleep = false;
bool LEDManager::isFadingFromSleep = false;
unsigned long LEDManager::sleepFadeStartTime = 0;
LEDEffect LEDManager::savedEffect = LED_EFFECT_NONE;
unsigned long LEDManager::rotateActivationTime = 0;  // Temps d'activation de l'effet ROTATE pour désactivation automatique
uint32_t LEDManager::sleepTimeoutMs = 0;
bool LEDManager::sleepPrevented = false;
bool LEDManager::pulseNeedsReset = false;
bool LEDManager::hardwareInitialized = false;
bool LEDManager::testSequentialActive = false;
int LEDManager::testSequentialIndex = 0;
unsigned long LEDManager::testSequentialLastUpdate = 0;

bool LEDManager::init() {
  Serial.println("[LED] Debut init...");
  Serial.printf("[LED] LED_DATA_PIN=%d, NUM_LEDS=%d\n", LED_DATA_PIN, NUM_LEDS);
  
  if (initialized) {
    Serial.println("[LED] Deja initialise");
    return true;
  }
  
  // Récupérer la configuration globale
  const SDConfig& config = InitManager::getConfig();
  currentBrightness = config.led_brightness;
  sleepTimeoutMs = config.sleep_timeout_ms;
  lastActivityTime = millis();
  isSleeping = false;
  Serial.printf("[LED] Brightness=%d, SleepTimeout=%lu\n", currentBrightness, sleepTimeoutMs);
  
  // Créer l'objet NeoPixel (l'initialisation matérielle sera faite dans la task)
  // NEO_GRB pour WS2812B (ordre des couleurs GRB)
  Serial.println("[LED] Creation objet NeoPixel...");
  strip = new Adafruit_NeoPixel(NUM_LEDS, LED_DATA_PIN, NEO_GRB + NEO_KHZ800);
  
  if (!strip) {
    Serial.println("[LED] ERREUR: Allocation memoire echouee!");
    return false;
  }
  Serial.println("[LED] Objet NeoPixel OK");
  
  // Ne PAS initialiser NeoPixel.begin() ici : cela peut nécessiter le scheduler
  // L'init matérielle NeoPixel est faite dans ledTask() au premier run.
  Serial.println("[LED] Init NeoPixel differe (dans task)...");
  
  // Créer la queue de commandes
  Serial.println("[LED] Creation queue...");
  commandQueue = xQueueCreate(QUEUE_SIZE, sizeof(LEDCommand));
  if (commandQueue == nullptr) {
    Serial.println("[LED] ERREUR: Creation queue echouee!");
    delete strip;
    strip = nullptr;
    return false;
  }
  Serial.println("[LED] Queue OK");
  
  // Créer le thread de gestion des LEDs sur Core 1 (temps-réel)
  Serial.println("[LED] Creation task...");
  Serial.printf("[LED] Core=%d, Priority=%d, Stack=%d\n", TASK_CORE, TASK_PRIORITY, TASK_STACK_SIZE);
  BaseType_t result = xTaskCreatePinnedToCore(
    ledTask,
    "LEDTask",
    TASK_STACK_SIZE,
    nullptr,
    TASK_PRIORITY,
    &taskHandle,
    TASK_CORE  // Core 1 (configuré dans core_config.h)
  );
  
  if (result != pdPASS) {
    Serial.printf("[LED] ERREUR: Creation task echouee! Code=%d\n", result);
    vQueueDelete(commandQueue);
    commandQueue = nullptr;
    delete strip;
    strip = nullptr;
    return false;
  }
  Serial.println("[LED] Task OK");
  
  initialized = true;
  
  // Laisser la tâche LED faire l'init NeoPixel avant d'envoyer des commandes
  vTaskDelay(pdMS_TO_TICKS(50));  // Attendre que la task ait fini son init
  
  // Éteindre toutes les LEDs au démarrage
  clear();
  
  Serial.println("[LED] Init complete!");
  return true;
}

void LEDManager::stop() {
  if (!initialized) {
    return;
  }
  
  // Ne devrait jamais être appelé, mais au cas où...
  if (taskHandle != nullptr) {
    vTaskDelete(taskHandle);
    taskHandle = nullptr;
  }
  
  if (commandQueue != nullptr) {
    vQueueDelete(commandQueue);
    commandQueue = nullptr;
  }
  
  if (strip != nullptr) {
    delete strip;
    strip = nullptr;
  }
  
  initialized = false;
  hardwareInitialized = false;
  Serial.println("[LED] Gestionnaire arrete (ne devrait pas arriver)");
}

bool LEDManager::sendCommand(const LEDCommand& cmd) {
  if (!initialized || commandQueue == nullptr) {
    return false;
  }
  
  // Envoyer la commande à la queue (non-bloquant)
  BaseType_t result = xQueueSend(commandQueue, &cmd, 0);
  return (result == pdTRUE);
}

bool LEDManager::setColor(uint8_t r, uint8_t g, uint8_t b) {
  Serial.printf("[LED] setColor: RGB(%d, %d, %d), sleepState=%d\n", r, g, b, getSleepState() ? 1 : 0);
  
  bool isTurningOff = (r == 0 && g == 0 && b == 0);
  
  // Ne pas faire clear() ici car cela effacerait la couleur avant qu'elle soit appliquée
  // Le clear() sera fait dans setEffect() si nécessaire lors d'un changement d'effet
  
  LEDCommand cmd;
  cmd.type = LED_CMD_SET_COLOR;
  cmd.data.color.r = r;
  cmd.data.color.g = g;
  cmd.data.color.b = b;
  bool result = sendCommand(cmd);
  
  // Réveiller automatiquement les LEDs pour tous les changements de couleur (sauf éteindre)
  // Cela permet de sortir du mode sommeil à chaque changement
  if (result && !isTurningOff) {
    wakeUp();
  } else if (isTurningOff) {
    Serial.println("[LED] setColor: Couleur noire detectee, pas de reveil");
  }
  return result;
}

bool LEDManager::setBrightness(uint8_t brightness) {
  LEDCommand cmd;
  cmd.type = LED_CMD_SET_BRIGHTNESS;
  cmd.data.brightness = brightness;
  bool result = sendCommand(cmd);
  // Réveiller automatiquement les LEDs pour tous les changements de brightness
  // Cela permet de sortir du mode sommeil à chaque changement
  if (result) {
    wakeUp();
  }
  return result;
}

const char* LEDManager::getEffectName(LEDEffect effect) {
  static const char* effectNames[] = {"NONE", "RAINBOW", "PULSE", "GLOSSY", "ROTATE", "NIGHTLIGHT", "BREATHE", "RAINBOW_SOFT"};
  if (effect >= 0 && effect < sizeof(effectNames) / sizeof(effectNames[0])) {
    return effectNames[effect];
  }
  return "UNKNOWN";
}

bool LEDManager::setEffect(LEDEffect effect) {
  Serial.printf("[LED] setEffect: %s, sleepState=%d\n", getEffectName(effect), getSleepState() ? 1 : 0);
  
  bool isTurningOff = (effect == LED_EFFECT_NONE);
  
  // Ne pas faire clear() ici car processCommand() gère déjà les transitions d'effet proprement
  // Il éteint automatiquement les LEDs avant de changer d'effet (voir processCommand SET_EFFECT)
  
  LEDCommand cmd;
  cmd.type = LED_CMD_SET_EFFECT;
  cmd.data.effect = effect;
  bool result = sendCommand(cmd);
  
  // Réveiller automatiquement les LEDs pour tous les changements d'effet (sauf éteindre)
  // MAIS seulement si on est vraiment en sleep mode pour éviter les flashes inutiles
  if (result && !isTurningOff) {
    // Ne réveiller que si on est vraiment en sleep mode
    // Sinon, on laisse le thread LED gérer directement le changement d'effet
    if (getSleepState()) {
      wakeUp();
    }
  } else if (isTurningOff) {
    Serial.println("[LED] setEffect: Effet NONE detecte, pas de reveil");
  }
  return result;
}

bool LEDManager::clear() {
  LEDCommand cmd;
  cmd.type = LED_CMD_CLEAR;
  return sendCommand(cmd);
}

bool LEDManager::isInitialized() {
  return initialized;
}

uint8_t LEDManager::getCurrentBrightness() {
  return currentBrightness;
}

bool LEDManager::testLEDsSequential() {
  if (!initialized) {
    Serial.println("[LED-TEST] LED Manager non initialise");
    return false;
  }
  
  Serial.println("[LED-TEST] Demarrage du test sequentiel des LEDs");
  
  // Envoyer une commande spéciale pour le test séquentiel
  LEDCommand cmd;
  cmd.type = LED_CMD_TEST_SEQUENTIAL;
  return sendCommand(cmd);
}

void LEDManager::ledTask(void* parameter) {
  // Init matérielle NeoPixel au premier run
  if (!hardwareInitialized) {
    if (strip != nullptr) {
      strip->begin();
      strip->setBrightness(currentBrightness);
      strip->clear();
      strip->show();
      hardwareInitialized = true;
    }
  }

  // Ce thread tourne en continu et ne s'arrête jamais
  // IMPORTANT: On limite les appels à strip->show() pour ne pas interférer avec l'audio I2S
  // strip->show() peut désactiver brièvement les interruptions, ce qui peut causer des grésillements
  
  static unsigned long lastShowTime = 0;
  static bool needsUpdate = true;  // Flag pour savoir si on doit appeler strip->show()
  
  // Intervalle minimum entre deux strip->show() (en ms)
  // 33ms = ~30 FPS, suffisant pour les animations et évite les conflits avec I2S
  const unsigned long SHOW_INTERVAL_MS = 33;
  
  while (true) {
    // Traiter les commandes en attente
    LEDCommand cmd;
    while (xQueueReceive(commandQueue, &cmd, 0) == pdTRUE) {
      processCommand(cmd);
      // IMPORTANT: Ne pas appeler wakeUp() automatiquement ici
      // wakeUp() est appelé uniquement par les méthodes publiques (setColor, setEffect, etc.)
      // Cela évite que les commandes système automatiques (WiFi retry, etc.) réveillent les LEDs
      // Si on est en sleep, les commandes sont traitées mais ne réveillent pas les LEDs
      needsUpdate = true;
    }
    
    // Gérer l'animation de fade depuis sleep (réveil) AVANT checkSleepMode()
    // Cela permet de réinitialiser lastActivityTime avant que checkSleepMode() ne vérifie le timeout
    if (isFadingFromSleep) {
      updateWakeFade();
      needsUpdate = true;
    }
    
    // Désactiver automatiquement l'effet ROTATE de validation après 8 secondes
    // Cela permet au sleep mode de se déclencher normalement après le démarrage
    if (currentEffect == LED_EFFECT_ROTATE && rotateActivationTime > 0) {
      unsigned long currentTime = millis();
      const unsigned long ROTATE_VALIDATION_TIMEOUT_MS = 8000;  // 8 secondes
      
      // Gérer le wrap-around de millis()
      unsigned long elapsed;
      if (currentTime >= rotateActivationTime) {
        elapsed = currentTime - rotateActivationTime;
      } else {
        elapsed = ULONG_MAX - rotateActivationTime + currentTime;
      }
      
      if (elapsed >= ROTATE_VALIDATION_TIMEOUT_MS) {
        Serial.println("[LED] Desactivation automatique de l'effet ROTATE de validation");
        currentEffect = LED_EFFECT_NONE;
        rotateActivationTime = 0;
        // Éteindre les LEDs pour permettre le sleep mode
        if (strip != nullptr) {
          for (int i = 0; i < NUM_LEDS; i++) {
            strip->setPixelColor(i, 0);
          }
        }
        needsUpdate = true;
      }
    }
    
    // Vérifier le sleep mode APRÈS avoir géré les animations de fade
    // Cela permet de s'assurer que lastActivityTime est à jour avant la vérification
    checkSleepMode();
    
    // Gérer le test séquentiel si actif
    if (testSequentialActive && strip != nullptr && hardwareInitialized) {
      // S'assurer que la luminosité est à 100% pendant le test
      strip->setBrightness(255);
      
      unsigned long currentTime = millis();
      if (currentTime - testSequentialLastUpdate >= 100) {  // 100ms entre chaque LED
        if (testSequentialIndex < NUM_LEDS) {
          // Phase 1: Allumer chaque LED une par une
          // Éteindre la LED précédente (sauf la première)
          if (testSequentialIndex > 0) {
            strip->setPixelColor(testSequentialIndex - 1, 0);
          }
          // Allumer la LED actuelle en blanc
          strip->setPixelColor(testSequentialIndex, strip->Color(255, 255, 255));
          strip->show();
          Serial.printf("[LED-TEST] LED %d/%d allumee\n", testSequentialIndex + 1, NUM_LEDS);
          testSequentialIndex++;
          testSequentialLastUpdate = currentTime;
          needsUpdate = true;
        } else if (testSequentialIndex == NUM_LEDS) {
          // Phase 2: Attendre 200ms avant d'allumer toutes en rouge
          if (currentTime - testSequentialLastUpdate >= 200) {
            // Éteindre la dernière LED
            strip->setPixelColor(NUM_LEDS - 1, 0);
            strip->show();
            testSequentialIndex++;
            testSequentialLastUpdate = currentTime;
            needsUpdate = true;
          }
        } else if (testSequentialIndex == NUM_LEDS + 1) {
          // Phase 3: Allumer toutes les LEDs en rouge
          for (int i = 0; i < NUM_LEDS; i++) {
            strip->setPixelColor(i, strip->Color(255, 0, 0)); // Rouge pur
          }
          strip->show();
          Serial.println("[LED-TEST] Test termine - Toutes les LEDs sont en rouge");
          Serial.println("[LED-TEST] Utilisez 'led clear' ou 'brightness 0' pour eteindre");
          testSequentialActive = false;  // Terminer le test
          currentColor = strip->Color(255, 0, 0);  // Sauvegarder la couleur rouge
          // Restaurer la luminosité configurée
          strip->setBrightness(currentBrightness);
          needsUpdate = true;
        }
      }
    }
    
    // Mettre à jour les effets animés si nécessaire
    // IMPORTANT: Les effets doivent continuer pendant le fade-out pour créer un fondu progressif
    // Seulement si le test séquentiel n'est pas actif
    if (!isSleeping && !testSequentialActive) {
      unsigned long currentTime = millis();
      if (currentTime - lastUpdateTime >= UPDATE_INTERVAL_MS) {
        // Pendant le fade-in, on permet les effets pour qu'ils s'appliquent progressivement
        // Mais on s'assure que les LEDs sont bien éteintes au début
        if (isFadingFromSleep && (currentTime - sleepFadeStartTime) < 50) {
          // Au tout début du fade-in, éteindre les LEDs pour éviter le flash
          if (strip != nullptr) {
            for (int i = 0; i < NUM_LEDS; i++) {
              strip->setPixelColor(i, 0);
            }
          }
        } else {
          // Appliquer les effets normalement (y compris pendant fade-out pour fondu progressif)
          updateEffects();
        }
        lastUpdateTime = currentTime;
        needsUpdate = true;
      }
      // S'assurer que la luminosité maximale configurée est toujours respectée
      // (sauf pendant le fade-in/fade-out où on utilise la luminosité fade)
      if (!isFadingFromSleep && !isFadingToSleep && strip != nullptr) {
        strip->setBrightness(currentBrightness);
      }
    }
    
    // Gérer l'animation de fade vers sleep APRÈS la mise à jour des effets
    // Cela permet aux effets de continuer pendant le fade-out avec luminosité réduite
    if (isFadingToSleep) {
      updateSleepFade();
      needsUpdate = true;
    }
    
    // Appliquer les changements aux LEDs SEULEMENT si nécessaire et pas trop souvent
    // Cela évite de bloquer les interruptions I2S trop fréquemment
    unsigned long currentTime = millis();
    if (needsUpdate && (currentTime - lastShowTime >= SHOW_INTERVAL_MS)) {
      // IMPORTANT: S'assurer que si on a clear() ou si l'effet est NONE avec couleur noire,
      // on éteint vraiment toutes les LEDs
      if (currentEffect == LED_EFFECT_NONE && currentColor == 0 && strip != nullptr) {
        // S'assurer que toutes les LEDs sont bien éteintes
        for (int i = 0; i < NUM_LEDS; i++) {
          strip->setPixelColor(i, 0);
        }
        // IMPORTANT: Mettre la luminosité à 0 pour éteindre complètement
        // Cela garantit que même si updateEffects() tourne, les LEDs restent éteintes
        strip->setBrightness(0);
      }
      if (strip != nullptr) {
        strip->show();
      }
      lastShowTime = currentTime;
      needsUpdate = false;
    }
    
    // Pause plus longue pour laisser de la bande passante à l'audio
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  
  // Ne devrait jamais arriver ici
  vTaskDelete(nullptr);
}

void LEDManager::processCommand(const LEDCommand& cmd) {
  switch (cmd.type) {
    case LED_CMD_SET_COLOR:
      Serial.printf("[LED] processCommand SET_COLOR: RGB(%d, %d, %d), currentEffect=%d\n", 
                    cmd.data.color.r, cmd.data.color.g, cmd.data.color.b, currentEffect);
      
      // Réinitialiser le timer d'activité lors d'un changement de couleur
      lastActivityTime = millis();
      
      // Si on change de couleur et qu'on a un effet actif, éteindre d'abord
      // Cela évite le flash de la couleur précédente
      if (currentEffect != LED_EFFECT_NONE && strip != nullptr) {
        // Éteindre toutes les LEDs avant de changer de couleur
        for (int i = 0; i < NUM_LEDS; i++) {
          strip->setPixelColor(i, 0);
        }
      }
      currentColor = ((uint32_t)cmd.data.color.r << 16) | ((uint32_t)cmd.data.color.g << 8) | cmd.data.color.b;
      
      // Si on définit la couleur SUCCESS (vert: RGB(0, 255, 0)) avec l'effet ROTATE,
      // démarrer le décompte pour désactivation automatique
      // Cela permet que le décompte ne commence qu'après la disparition de l'orange
      if (currentEffect == LED_EFFECT_ROTATE && 
          cmd.data.color.r == 0 && cmd.data.color.g == 255 && cmd.data.color.b == 0) {
        rotateActivationTime = millis();
        Serial.printf("[LED] processCommand SET_COLOR - Couleur SUCCESS (vert) detectee avec ROTATE, demarrage du decompte: %lu ms\n", rotateActivationTime);
      }
      
      // Si on change de couleur et qu'on n'a pas d'effet actif, appliquer immédiatement
      // Si on a un effet, la couleur sera appliquée par l'effet
      if (currentEffect == LED_EFFECT_NONE && strip != nullptr) {
        // Appliquer la couleur immédiatement
        for (int i = 0; i < NUM_LEDS; i++) {
          strip->setPixelColor(i, currentColor);
        }
      }
      // Ne pas réinitialiser l'effet ici, il sera géré par SET_EFFECT
      // L'effet reste actif, seule la couleur change
      break;
      
    case LED_CMD_SET_BRIGHTNESS:
      // Réinitialiser le timer d'activité lors d'un changement de luminosité
      lastActivityTime = millis();
      
      currentBrightness = cmd.data.brightness;
      if (strip != nullptr) {
        strip->setBrightness(currentBrightness);
        // Réappliquer la couleur sur toutes les LEDs si pas d'effet actif
        if (currentEffect == LED_EFFECT_NONE) {
          for (int i = 0; i < NUM_LEDS; i++) {
            strip->setPixelColor(i, currentColor);
          }
        }
      }
      break;
      
    case LED_CMD_SET_EFFECT: {
      Serial.printf("[LED] processCommand SET_EFFECT: %s (ancien: %s)\n", 
                    getEffectName(cmd.data.effect), getEffectName(currentEffect));
      
      // IMPORTANT: Réinitialiser le timer d'activité IMMÉDIATEMENT au début du traitement
      // Cela évite que checkSleepMode() (appelé dans la boucle principale) entre en sleep mode
      // pendant le traitement de la commande
      if (cmd.data.effect != LED_EFFECT_NONE) {
        lastActivityTime = millis();
      }
      
      // Si on change d'effet, éteindre d'abord pour transition propre
      // Cela évite le flash de la couleur/effet précédent
      if (currentEffect != cmd.data.effect && strip != nullptr) {
        // Éteindre toutes les LEDs avant de changer d'effet
        for (int i = 0; i < NUM_LEDS; i++) {
          strip->setPixelColor(i, 0);
        }
      }
      LEDEffect previousEffect = currentEffect;
      currentEffect = cmd.data.effect;
      
      // Note: rotateActivationTime sera défini explicitement lors du passage au vert (SUCCESS)
      // pour que le décompte ne commence qu'après la disparition de l'orange
      if (currentEffect != LED_EFFECT_ROTATE) {
        rotateActivationTime = 0;  // Réinitialiser si on change d'effet
      }
      // Si on change vers NONE, vérifier si on veut éteindre ou afficher une couleur fixe
      if (currentEffect == LED_EFFECT_NONE && strip != nullptr) {
        if (previousEffect != LED_EFFECT_NONE) {
          // Changement depuis un effet animé : éteindre visuellement pour transition propre
          // MAIS ne pas réinitialiser currentColor à 0, car setColor() peut être appelé après
          // pour afficher une couleur fixe avec LED_EFFECT_NONE
          // Note: On ne modifie PAS currentColor ni brightness ici
          for (int i = 0; i < NUM_LEDS; i++) {
            strip->setPixelColor(i, 0);
          }
          strip->show();
          Serial.println("[LED] processCommand SET_EFFECT NONE - Transition depuis effet anime, LEDs eteintes temporairement (couleur preservee)");
        }
        // Si on est déjà en mode NONE, ne pas réinitialiser la couleur
        // Cela permet d'afficher une couleur fixe avec LED_EFFECT_NONE
        // La couleur sera préservée et affichée si elle est définie
      }
      // Si on change vers PULSE, réinitialiser l'effet pour éviter le flash
      if (currentEffect == LED_EFFECT_PULSE) {
        resetPulseEffect();
        // IMPORTANT: S'assurer que currentColor est bien défini avant d'activer PULSE
        // Si currentColor est 0 (noir) ou contient une couleur résiduelle indésirable,
        // attendre que la couleur soit définie par setColor() avant d'activer PULSE
        // Note: Le code appelant devrait faire clear() + setColor() + delay() + setEffect()
        // pour s'assurer que la couleur est bien définie avant d'activer PULSE
        if (currentColor == 0 && strip != nullptr) {
          // Couleur non définie, s'assurer que les LEDs sont éteintes
          for (int i = 0; i < NUM_LEDS; i++) {
            strip->setPixelColor(i, 0);
          }
          strip->setBrightness(0);
          Serial.println("[LED] processCommand SET_EFFECT PULSE - Couleur non definie, LEDs eteintes");
        } else {
          // Couleur définie, PULSE utilisera cette couleur
          Serial.printf("[LED] processCommand SET_EFFECT PULSE - Couleur: RGB(%d, %d, %d)\n",
                       (currentColor >> 16) & 0xFF, (currentColor >> 8) & 0xFF, currentColor & 0xFF);
        }
      }
      // Les effets seront gérés par updateEffects()
      break;
    }
      
    case LED_CMD_CLEAR:
      Serial.println("[LED] processCommand CLEAR");
      currentColor = 0;  // Noir
      currentEffect = LED_EFFECT_NONE;
      testSequentialActive = false;  // Arrêter le test si en cours
      // IMPORTANT: Éteindre complètement toutes les LEDs
      if (strip != nullptr) {
        for (int i = 0; i < NUM_LEDS; i++) {
          strip->setPixelColor(i, 0);
        }
        // IMPORTANT: Mettre la luminosité à 0 pour éteindre complètement
        // Cela garantit que même si updateEffects() tourne, les LEDs restent éteintes
        strip->setBrightness(0);
      }
      // Réinitialiser l'effet PULSE si nécessaire pour éviter qu'il reprenne
      pulseNeedsReset = false;
      // La mise à jour sera faite par strip->show() dans la boucle principale
      // avec needsUpdate = true qui a été défini lors de la réception de la commande
      break;
      
    case LED_CMD_TEST_SEQUENTIAL:
      Serial.println("[LED] processCommand TEST_SEQUENTIAL");
      Serial.printf("[LED-TEST] Nombre total de LEDs: %d\n", NUM_LEDS);
      // Réveiller les LEDs si elles sont en sleep
      if (isSleeping) {
        wakeUp();
      }
      // Désactiver les effets temporairement
      currentEffect = LED_EFFECT_NONE;
      // Initialiser le test séquentiel
      testSequentialActive = true;
      testSequentialIndex = 0;
      testSequentialLastUpdate = millis();
      // Éteindre toutes les LEDs au début
      if (strip != nullptr) {
        for (int i = 0; i < NUM_LEDS; i++) {
          strip->setPixelColor(i, 0);
        }
        strip->show();
      }
      Serial.println("[LED-TEST] Test sequentiel demarre");
      break;
  }
  
  // IMPORTANT: Ne PAS mettre à jour lastActivityTime ici
  // lastActivityTime est mis à jour uniquement dans wakeUp() et dans les méthodes publiques
  // Cela évite que les commandes système (WiFi retry, etc.) empêchent le sleep mode
}

void LEDManager::checkSleepMode() {
  // Si le sleep mode est désactivé (timeout = 0), ne rien faire
  if (sleepTimeoutMs == 0) {
    if (isSleeping || isFadingToSleep) {
      // Réveiller si on était en sleep ou en fade
      isSleeping = false;
      isFadingToSleep = false;
      if (strip != nullptr) {
        strip->setBrightness(currentBrightness);
      }
    }
    return;
  }
  
  // IMPORTANT: Ne pas entrer en sleep mode si le sleep est empêché (bedtime, etc.)
  if (sleepPrevented) {
    // Sleep empêché, réveiller si on était en sleep
    if (isSleeping || isFadingToSleep) {
      isSleeping = false;
      isFadingToSleep = false;
      if (strip != nullptr) {
        strip->setBrightness(currentBrightness);
      }
      // Restaurer l'effet si nécessaire
      if (savedEffect != LED_EFFECT_NONE) {
        currentEffect = savedEffect;
        savedEffect = LED_EFFECT_NONE;
      }
      // Réinitialiser le timer d'activité UNIQUEMENT si on réveille depuis le sleep
      lastActivityTime = millis();
    }
    return;
  }
  
  // IMPORTANT: Ne pas entrer en sleep mode si le BLE est actif (mode appairage)
  // Les LEDs doivent rester allumées pour indiquer le mode appairage
  #ifdef HAS_BLE
  if (BLEConfigManager::isBLEEnabled()) {
    // BLE actif, réveiller si on était en sleep
    if (isSleeping || isFadingToSleep) {
      isSleeping = false;
      isFadingToSleep = false;
      if (strip != nullptr) {
        strip->setBrightness(currentBrightness);
      }
      // Restaurer l'effet si nécessaire
      if (savedEffect != LED_EFFECT_NONE) {
        currentEffect = savedEffect;
        savedEffect = LED_EFFECT_NONE;
      }
    }
    // IMPORTANT: Réinitialiser le timer d'activité pour empêcher le sleep mode
    // Le sleep mode ne doit jamais éteindre les LEDs quand le BLE est actif (mode appairage)
    lastActivityTime = millis();
    return;
  }
  #endif
  
  // IMPORTANT: Ne pas entrer en sleep mode si un effet animé est actif
  // Les effets animés (RAINBOW, PULSE, GLOSSY, ROTATE, NIGHTLIGHT, BREATHE) sont des activités actives
  // qui devraient empêcher le sleep mode
  // LED_EFFECT_NONE avec une couleur fixe peut entrer en sleep mode normalement
  bool hasActiveAnimatedEffect = (currentEffect != LED_EFFECT_NONE);
  
  unsigned long currentTime = millis();
  unsigned long timeSinceActivity = currentTime - lastActivityTime;
  
  // Vérifier si on doit entrer en sleep mode
  // Ne pas entrer en sleep mode si un effet animé est actif (mais LED_EFFECT_NONE peut entrer en sleep)
  if (!isSleeping && !isFadingToSleep && !hasActiveAnimatedEffect && timeSinceActivity >= sleepTimeoutMs) {
    // Démarrer l'animation de fade vers sleep
    Serial.printf("[LED] Entree en sleep mode (timeout: %lu ms, inactivite: %lu ms, lastActivityTime=%lu, currentTime=%lu)\n", 
                  sleepTimeoutMs, timeSinceActivity, lastActivityTime, currentTime);
    isFadingToSleep = true;
    sleepFadeStartTime = currentTime;
    // Sauvegarder l'effet actuel pour le restaurer au réveil
    savedEffect = currentEffect;
    Serial.printf("[LED] Effet sauvegarde: %d\n", savedEffect);
  }
}

void LEDManager::updateSleepFade() {
  unsigned long currentTime = millis();
  unsigned long elapsed = currentTime - sleepFadeStartTime;
  
  if (elapsed >= SLEEP_FADE_DURATION_MS) {
    // Animation terminée, éteindre complètement
    isFadingToSleep = false;
    isSleeping = true;
    if (strip != nullptr) {
      strip->setBrightness(0);
      // IMPORTANT: Clear les LEDs quand on atteint 0 luminosité (mode sleep)
      // Cela évite le flash de la couleur précédente quand on réactive
      for (int i = 0; i < NUM_LEDS; i++) {
        strip->setPixelColor(i, 0);
      }
    }
  } else {
    // Calculer le facteur de fade (1.0 -> 0.0)
    float fadeFactor = 1.0f - ((float)elapsed / (float)SLEEP_FADE_DURATION_MS);
    
    // Appliquer le fondu progressif en baissant la luminosité globale
    // Les effets continuent de s'afficher (gérés dans la boucle principale) mais avec luminosité réduite
    uint8_t fadedBrightness = (uint8_t)(currentBrightness * fadeFactor);
    if (strip != nullptr) {
      strip->setBrightness(fadedBrightness);
      // Ne pas clear les LEDs ici, laisser l'effet continuer avec luminosité réduite
      // Cela crée un fondu progressif naturel
    }
  }
}

void LEDManager::wakeUp() {
  bool wasSleeping = (isSleeping || isFadingToSleep);
  
  if (isSleeping || isFadingToSleep) {
    Serial.printf("[LED] wakeUp() - Reveil depuis sleep (wasSleeping=%d, savedEffect=%d, currentColor=0x%06X)\n", 
                  wasSleeping ? 1 : 0, savedEffect, currentColor);
    isSleeping = false;
    isFadingToSleep = false;
    
    // IMPORTANT: Éteindre d'abord les LEDs pour éviter le flash de l'animation précédente
    // Les LEDs peuvent encore contenir l'ancienne couleur/effet
    if (strip != nullptr) {
      for (int i = 0; i < NUM_LEDS; i++) {
        strip->setPixelColor(i, 0);
      }
    }
    
    // Démarrer un fade-in progressif pour le réveil
    isFadingFromSleep = true;
    sleepFadeStartTime = millis();
    
    // Restaurer l'effet s'il y en avait un
    if (savedEffect != LED_EFFECT_NONE) {
      Serial.printf("[LED] wakeUp() - Restauration effet: %s\n", getEffectName(savedEffect));
      currentEffect = savedEffect;
      // Si on restaure PULSE, réinitialiser l'effet
      if (currentEffect == LED_EFFECT_PULSE) {
        resetPulseEffect();
      }
    } else {
      // Pas d'effet sauvegardé -> ne rien restaurer, garder l'état actuel
      // Cela évite les flashes inutiles quand wakeUp() est appelé sans effet sauvegardé
      Serial.println("[LED] wakeUp() - Pas d'effet sauvegarde, conservation de l'etat actuel");
      // Ne pas modifier currentEffect ni currentColor, ils seront mis à jour par la prochaine commande
    }
  }
  
  // TOUJOURS réinitialiser le timer d'activité quand wakeUp() est appelé
  // Cela permet de tester les effets via Serial sans que le sleep mode se réactive immédiatement
  // et garantit que le système reste actif après un réveil explicite
  lastActivityTime = millis();
  
  // NOTE: Ne pas démarrer automatiquement le WiFi retry depuis wakeUp()
  // car cela peut créer un cycle : WiFi retry -> commande LED -> wakeUp() -> WiFi retry
  // Le WiFi retry doit être géré indépendamment par le système d'initialisation
}

void LEDManager::preventSleep() {
  sleepPrevented = true;
  // Réveiller immédiatement si on était en sleep
  if (isSleeping || isFadingToSleep) {
    isSleeping = false;
    isFadingToSleep = false;
    if (strip != nullptr) {
      strip->setBrightness(currentBrightness);
    }
    // Restaurer l'effet si nécessaire
    if (savedEffect != LED_EFFECT_NONE) {
      currentEffect = savedEffect;
      savedEffect = LED_EFFECT_NONE;
    }
    lastActivityTime = millis();
  }
  Serial.println("[LED] Sleep mode empeche (bedtime actif)");
}

void LEDManager::allowSleep() {
  sleepPrevented = false;
  Serial.println("[LED] Sleep mode reautorise");
}

void LEDManager::updateWakeFade() {
  unsigned long currentTime = millis();
  unsigned long elapsed = currentTime - sleepFadeStartTime;
  
  if (elapsed >= SLEEP_FADE_DURATION_MS) {
    // Animation terminée, restaurer complètement
    Serial.printf("[LED] updateWakeFade() - Animation reveil terminee, effet=%s, couleur=0x%06X\n",
                  getEffectName(currentEffect), currentColor);
    isFadingFromSleep = false;
    
    // IMPORTANT: Réinitialiser le timer d'activité quand l'animation de réveil se termine
    // Cela évite que le sleep mode se réactive immédiatement après le réveil
    lastActivityTime = millis();
    
    // IMPORTANT: S'assurer que les LEDs sont bien éteintes avant de restaurer l'effet
    // Cela évite le flash de l'animation précédente
    if (strip != nullptr) {
      for (int i = 0; i < NUM_LEDS; i++) {
        strip->setPixelColor(i, 0);
      }
      
      // Si l'effet est PULSE, réinitialiser pour une transition fluide
      if (currentEffect == LED_EFFECT_PULSE) {
        resetPulseEffect();
        // Réinitialiser lastUpdateTime pour que l'effet reprenne immédiatement
        lastUpdateTime = millis();
      }
      
      // Restaurer la luminosité complète
      strip->setBrightness(currentBrightness);
      
      // S'assurer que toutes les LEDs ont la couleur si pas d'effet
      if (currentEffect == LED_EFFECT_NONE) {
        for (int i = 0; i < NUM_LEDS; i++) {
          strip->setPixelColor(i, currentColor);
        }
      }
    }
  } else {
    // Calculer le facteur de fade (0.0 -> 1.0)
    float fadeFactor = (float)elapsed / (float)SLEEP_FADE_DURATION_MS;
    
    // Simple: on remonte juste la luminosité globale
    uint8_t fadedBrightness = (uint8_t)(currentBrightness * fadeFactor);
    if (strip != nullptr) {
      strip->setBrightness(fadedBrightness);
      
      // IMPORTANT: Pendant le fade-in, s'assurer que les LEDs sont bien éteintes au début
      // et appliquer la nouvelle couleur/effet progressivement
      if (fadedBrightness == 0 || elapsed < 50) {
        // Au tout début du fade-in, s'assurer que les LEDs sont bien éteintes
        for (int i = 0; i < NUM_LEDS; i++) {
          strip->setPixelColor(i, 0);
        }
      } else {
        // Pendant le fade-in, appliquer la couleur/effet
        // Si on a un effet, il sera géré par updateEffects() dans la boucle principale
        // Mais on doit s'assurer que la couleur de base est correcte
        if (currentEffect == LED_EFFECT_NONE) {
          for (int i = 0; i < NUM_LEDS; i++) {
            strip->setPixelColor(i, currentColor);
          }
        }
        // Si on a un effet, updateEffects() s'en chargera dans la boucle principale
        // Mais on doit permettre à updateEffects() de s'exécuter même pendant le fade-in
      }
    }
  }
}

bool LEDManager::getSleepState() {
  // Retourner true si on est en sleep OU en fade vers sleep
  // Cela évite de réveiller les LEDs si elles sont en train de s'éteindre
  return isSleeping || isFadingToSleep;
}

void LEDManager::resetPulseEffect() {
  // Marquer que l'effet PULSE doit être réinitialisé
  pulseNeedsReset = true;
}

void LEDManager::updateEffects() {
  static unsigned long effectTime = 0;
  unsigned long currentTime = millis();
  
  switch (currentEffect) {
    case LED_EFFECT_NONE:
      // Pas d'effet, couleur unie déjà appliquée
      break;
      
    case LED_EFFECT_RAINBOW: {
      // Effet arc-en-ciel qui défile
      static uint8_t hue = 0;
      if (strip != nullptr) {
        for (int i = 0; i < NUM_LEDS; i++) {
          uint32_t color = hsvToRgb((hue + i * 2) % 256, 255, 255);
          strip->setPixelColor(i, color);
        }
      }
      hue = (hue + 2) % 256;
      break;
    }
    
    case LED_EFFECT_RAINBOW_SOFT: {
      // Effet arc-en-ciel doux et lent pour veilleuse
      // Animation beaucoup plus lente que RAINBOW standard
      static unsigned long rainbowSoftStartTime = 0;
      
      // Initialiser le temps de départ si nécessaire
      if (rainbowSoftStartTime == 0) {
        rainbowSoftStartTime = currentTime;
      }
      
      // Cycle complet de l'arc-en-ciel : ~30 secondes pour un tour complet (beaucoup plus lent)
      const uint32_t RAINBOW_SOFT_CYCLE_MS = 30000;  // 30 secondes
      
      // Gérer le wrap-around de millis()
      uint32_t elapsed;
      if (currentTime >= rainbowSoftStartTime) {
        elapsed = (currentTime - rainbowSoftStartTime) % RAINBOW_SOFT_CYCLE_MS;
      } else {
        elapsed = (ULONG_MAX - rainbowSoftStartTime + currentTime) % RAINBOW_SOFT_CYCLE_MS;
      }
      
      // Calculer la teinte de base avec précision (0 à 255)
      // Utiliser une précision élevée pour fluidité maximale
      uint16_t baseHue = (elapsed * 256) / RAINBOW_SOFT_CYCLE_MS;
      
      if (strip != nullptr) {
        // Répartir l'arc-en-ciel sur toute la bande LED
        // Chaque LED a une teinte légèrement différente pour créer un dégradé
        for (int i = 0; i < NUM_LEDS; i++) {
          // Calculer la teinte pour cette LED (dégradé sur toute la bande)
          // Utiliser une répartition douce pour un effet apaisant
          uint16_t hue = (baseHue + (i * 256 / NUM_LEDS)) % 256;
          
          // Saturation et luminosité réduites pour un effet plus doux et apaisant
          // Saturation: 200/255 (78%) pour des couleurs moins vives
          // Luminosité: 180/255 (70%) pour un effet plus doux
          uint32_t color = hsvToRgb((uint8_t)hue, 200, 180);
          strip->setPixelColor(i, color);
        }
      }
      break;
    }
    
    case LED_EFFECT_PULSE: {
      // Effet de pulsation (respiration) rapide et fluide
      // Utiliser le temps réel pour une vitesse constante et fluide
      static unsigned long pulseStartTime = 0;
      
      // Réinitialiser si nécessaire (après réveil depuis sleep)
      if (pulseNeedsReset) {
        pulseStartTime = currentTime;
        pulseNeedsReset = false;
      }
      
      // Cycle de respiration : ~2.5 secondes pour un cycle complet (inspiration + expiration)
      const uint32_t PULSE_CYCLE_MS = 2500;  // 2.5 secondes
      uint32_t elapsed = (currentTime - pulseStartTime) % PULSE_CYCLE_MS;
      
      // Calculer la phase normalisée (0 à 1023 pour précision)
      uint16_t phase = (elapsed * 1024) / PULSE_CYCLE_MS;
      
      // Calculer la valeur de pulsation avec une courbe sinusoïdale douce
      // Utiliser une approximation de sin pour fluidité maximale
      // Phase 0-511 : montée (inspiration), Phase 512-1023 : descente (expiration)
      // Minimum réduit de 50 à 30 pour un effet plus sombre en bas (~12% de luminosité)
      const uint8_t PULSE_MIN = 30;  // Minimum de luminosité (réduit de 50 à 30)
      const uint8_t PULSE_MAX = 255;  // Maximum de luminosité
      const uint8_t PULSE_RANGE = PULSE_MAX - PULSE_MIN;  // 225
      
      uint8_t pulseValue;
      if (phase < 512) {
        // Montée (inspiration) : de PULSE_MIN à PULSE_MAX
        // Courbe douce : utiliser phase² pour accélération progressive
        uint16_t normalizedPhase = phase;  // 0-511
        // Appliquer une courbe douce (quadratique) pour fluidité
        uint16_t smoothPhase = (normalizedPhase * normalizedPhase) / 512;
        pulseValue = PULSE_MIN + ((smoothPhase * PULSE_RANGE) / 512);
      } else {
        // Descente (expiration) : de PULSE_MAX à PULSE_MIN
        uint16_t phaseDown = phase - 512;  // 0-511
        // Courbe douce inverse
        uint16_t smoothPhase = 511 - phaseDown;
        smoothPhase = (smoothPhase * smoothPhase) / 512;
        pulseValue = PULSE_MIN + ((smoothPhase * PULSE_RANGE) / 512);
      }
      
      // Appliquer la pulsation à la couleur
      if (strip != nullptr) {
        // Extraire les composantes RGB de currentColor
        uint8_t r = (currentColor >> 16) & 0xFF;
        uint8_t g = (currentColor >> 8) & 0xFF;
        uint8_t b = currentColor & 0xFF;
        
        // Appliquer la pulsation (fade)
        r = (r * pulseValue) / 255;
        g = (g * pulseValue) / 255;
        b = (b * pulseValue) / 255;
        
        uint32_t pulseColor = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        for (int i = 0; i < NUM_LEDS; i++) {
          strip->setPixelColor(i, pulseColor);
        }
      }
      break;
    }
    
    case LED_EFFECT_GLOSSY: {
      // Effet glossy multicolore
      static uint8_t offset = 0;
      if (strip != nullptr) {
        for (int i = 0; i < NUM_LEDS; i++) {
          uint8_t hue = ((i * 256 / NUM_LEDS) + offset) % 256;
          uint32_t color = hsvToRgb(hue, 200, 255);
          strip->setPixelColor(i, color);
        }
      }
      offset = (offset + 1) % 256;
      break;
    }
    
    case LED_EFFECT_ROTATE: {
      // Effet de rotation type "serpent" avec début (tête) et fin (queue) progressifs
      // Utiliser le temps réel pour une rotation constante et fluide
      static unsigned long rotateStartTime = 0;
      
      // Initialiser le temps de départ si nécessaire
      if (rotateStartTime == 0) {
        rotateStartTime = currentTime;
      }
      
      // Cycle de rotation : ~5 secondes pour un tour complet (ralenti pour plus de fluidité)
      // Plus lent = chaque LED reste allumée plus longtemps = animation plus fluide
      const uint32_t ROTATE_CYCLE_MS = 5000;  // 5 secondes (au lieu de 4)
      uint32_t elapsed = (currentTime - rotateStartTime) % ROTATE_CYCLE_MS;
      
      // Calculer la position de la tête du serpent avec précision fractionnaire
      // Utiliser une précision élevée (256x) pour fluidité maximale
      uint32_t headPositionPrecise = (elapsed * NUM_LEDS * 256) / ROTATE_CYCLE_MS;
      uint16_t headPosition = (headPositionPrecise >> 8) % NUM_LEDS;  // Position de la tête (LED principale)
      uint8_t headSubPosition = headPositionPrecise & 0xFF;  // Position fractionnaire (0-255)
      
      // Longueur du serpent (nombre de LEDs) - queue optimale avec fade-out très progressif
      // 30% de la bande pour queue visible mais pas trop longue
      // Le fade-out progressif fait que les dernières LEDs restent allumées longtemps
      const uint8_t snakeLength = (NUM_LEDS * 30) / 100;  // 30% de la bande (optimal)
      
      // Éteindre toutes les LEDs
      if (strip != nullptr) {
        for (int i = 0; i < NUM_LEDS; i++) {
          strip->setPixelColor(i, 0);
        }
      }
      
      // Dessiner le serpent progressif avec position fractionnaire pour mouvement ultra-fluide
      // Le serpent s'étend de (headPosition - snakeLength) à headPosition
      // Utiliser la position fractionnaire pour créer un dégradé qui se déplace progressivement
      
      // Parcourir toutes les LEDs pour créer un dégradé fluide
      for (int ledIndex = 0; ledIndex < NUM_LEDS; ledIndex++) {
        // Calculer la position précise de cette LED (en unités fractionnaires)
        // Utiliser le centre de la LED pour plus de précision et fluidité
        // +128 pour centrer, cela donne une transition plus douce entre LEDs
        int32_t ledPositionPrecise = ((int32_t)ledIndex * 256) + 128;  // Centre de la LED
        
        // Calculer la distance depuis la tête (en tenant compte du wrap-around)
        int32_t distanceToHead = headPositionPrecise - ledPositionPrecise;
        
        // Gérer le wrap-around (distance la plus courte)
        if (distanceToHead > (NUM_LEDS * 256) / 2) {
          distanceToHead -= (NUM_LEDS * 256);
        } else if (distanceToHead < -(NUM_LEDS * 256) / 2) {
          distanceToHead += (NUM_LEDS * 256);
        }
        
        // Convertir en valeur absolue et vérifier si cette LED fait partie du serpent
        int32_t absDistance = (distanceToHead < 0) ? -distanceToHead : distanceToHead;
        int32_t maxSnakeDistance = snakeLength * 256;
        
        // Si la LED est en dehors du serpent, elle est éteinte
        if (absDistance > maxSnakeDistance) {
          if (strip != nullptr) {
            strip->setPixelColor(ledIndex, 0);
          }
          continue;
        }
        
        // Calculer l'intensité avec dégradé très progressif de la queue vers la tête
        // Queue très longue qui reste allumée longtemps avec luminosité qui diminue très doucement
        uint8_t intensity;
        if (absDistance == 0) {
          // Tête : luminosité maximale (255)
          intensity = 255;
        } else {
          // Queue vers tête : dégradé très progressif avec courbe exponentielle douce
          // Utiliser la distance inverse : proche de la tête = plus élevé
          uint32_t fadeFactor = maxSnakeDistance - absDistance;
          
          // Normaliser sur 0-256 pour calcul de courbe
          uint32_t normalizedFade = (fadeFactor * 256) / maxSnakeDistance;  // 0-256
          
          // Courbe optimisée : queue qui s'éteint rapidement mais dernières LEDs restent visibles
          // avec fade-out progressif pour fluidité maximale
          uint32_t x = normalizedFade;  // 0-256
          
          // Courbe en 3 zones pour fade-out progressif mais queue qui s'éteint assez vite
          // - Queue lointaine (x < 80) : s'éteint rapidement (presque invisible)
          // - Queue moyenne (80 < x < 180) : fade-out progressif (visible mais faible)
          // - Queue proche vers tête (x > 180) : montée rapide vers maximum
          
          uint32_t fadeValue;
          
          if (x < 80) {
            // Queue très lointaine (x = 0-80) : s'éteint rapidement
            // Courbe quadratique pour extinction rapide mais douce
            uint32_t xNormalized = x;  // 0-80
            fadeValue = (xNormalized * xNormalized) / 80;  // 0-80, extinction rapide
          } else if (x < 180) {
            // Queue moyenne (x = 80-180) : fade-out progressif
            // Les dernières LEDs restent visibles avec luminosité qui diminue doucement
            uint32_t xNormalized = x - 80;  // 0-100
            uint32_t baseValue = 80;  // Valeur de base depuis la queue lointaine
            // Courbe quadratique douce pour fade-out progressif
            uint32_t increment = (xNormalized * xNormalized) / 40;  // 0-250
            fadeValue = baseValue + increment;  // 80-330
          } else {
            // Queue proche et tête (x = 180-256) : montée rapide vers maximum
            uint32_t xNormalized = x - 180;  // 0-76
            uint32_t baseValue = 330;  // Valeur de base depuis la queue moyenne
            // Courbe cubique pour montée rapide mais fluide vers la tête
            uint32_t increment = (xNormalized * xNormalized * xNormalized) / 300;  // 0-76
            fadeValue = baseValue + increment;  // 330-406
          }
          
          // Limiter à 256 maximum
          if (fadeValue > 256) fadeValue = 256;
          
          // Intensité optimisée : queue s'éteint assez vite mais dernières LEDs restent visibles
          // Queue lointaine : 15 (très faible, presque éteinte)
          // Queue moyenne : 15-150 (fade-out progressif, reste visible)
          // Queue proche : 150-255 (montée rapide vers tête)
          // Cela donne une queue qui s'éteint assez vite mais les dernières LEDs restent visibles
          intensity = 15 + ((fadeValue * 240) / 256);
          
          // S'assurer que l'intensité minimale est respectée
          if (intensity < 15) intensity = 15;
        }
        
        // Appliquer la couleur avec l'intensité calculée
        if (strip != nullptr) {
          // Extraire les composantes RGB de currentColor
          uint8_t r = (currentColor >> 16) & 0xFF;
          uint8_t g = (currentColor >> 8) & 0xFF;
          uint8_t b = currentColor & 0xFF;
          
          // Appliquer l'intensité (fade)
          r = (r * intensity) / 255;
          g = (g * intensity) / 255;
          b = (b * intensity) / 255;
          
          uint32_t snakeColor = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
          strip->setPixelColor(ledIndex, snakeColor);
        }
      }
      break;
    }
    
    case LED_EFFECT_NIGHTLIGHT: {
      // Effet de veilleuse avec vagues bleu/blanc qui se déplacent de gauche à droite
      // Utiliser le temps réel pour une animation constante et fluide
      static unsigned long nightlightStartTime = 0;
      
      // Initialiser le temps de départ si nécessaire
      if (nightlightStartTime == 0) {
        nightlightStartTime = currentTime;
      }
      
      // Cycle de déplacement : ~6 secondes pour traverser toute la bande
      const uint32_t NIGHTLIGHT_CYCLE_MS = 6000;  // 6 secondes
      
      // Gérer le wrap-around de millis()
      uint32_t elapsed;
      if (currentTime >= nightlightStartTime) {
        elapsed = (currentTime - nightlightStartTime) % NIGHTLIGHT_CYCLE_MS;
      } else {
        // Wrap-around détecté
        elapsed = (ULONG_MAX - nightlightStartTime + currentTime) % NIGHTLIGHT_CYCLE_MS;
      }
      
      // Calculer l'offset de déplacement (0 à NUM_LEDS * 2 pour permettre plusieurs cycles visuels)
      float scrollOffset = ((float)elapsed / (float)NIGHTLIGHT_CYCLE_MS) * (float)(NUM_LEDS * 2);
      
      if (strip != nullptr) {
        // Couleurs de base : bleu et blanc
        const uint8_t BLUE_R = 30;
        const uint8_t BLUE_G = 100;
        const uint8_t BLUE_B = 255;
        
        const uint8_t WHITE_R = 200;
        const uint8_t WHITE_G = 220;
        const uint8_t WHITE_B = 255;
        
        // Créer plusieurs vagues qui se déplacent de gauche à droite
        for (int i = 0; i < NUM_LEDS; i++) {
          // Position avec décalage pour créer le mouvement de gauche à droite
          float position = (float)i - scrollOffset;
          
          // Créer 3 vagues sinusoïdales avec différentes fréquences et phases
          // Vague 1 : longue période (bleu dominant) - mouvement lent
          float wave1 = sin((position / (float)NUM_LEDS) * 2.0 * M_PI * 1.5) * 0.5 + 0.5;
          
          // Vague 2 : période moyenne (blanc subtil) - mouvement moyen
          float wave2 = sin((position / (float)NUM_LEDS) * 2.0 * M_PI * 2.5 + (M_PI / 3.0)) * 0.5 + 0.5;
          
          // Vague 3 : courte période (accent bleu) - mouvement rapide
          float wave3 = sin((position / (float)NUM_LEDS) * 2.0 * M_PI * 4.0 + (M_PI / 2.0)) * 0.3 + 0.3;
          
          // Combiner les vagues (mélange bleu/blanc - bleu dominant)
          // Assurer un minimum de luminosité pour éviter les zones complètement sombres
          float blueFactor = wave1 * 0.6 + wave3 * 0.4;
          float whiteFactor = wave2 * 0.2;
          
          // Ajouter un fond bleu minimal pour éviter les zones complètement éteintes
          float baseBlue = 0.3; // Fond bleu minimal (30%)
          blueFactor = blueFactor * 0.7 + baseBlue; // 70% de la vague + 30% de fond
          
          // Normaliser pour éviter la saturation, mais garder un minimum
          float total = blueFactor + whiteFactor;
          if (total > 1.0) {
            blueFactor /= total;
            whiteFactor /= total;
          }
          
          // Calculer les composantes RGB finales
          uint8_t r = (uint8_t)(BLUE_R * blueFactor + WHITE_R * whiteFactor);
          uint8_t g = (uint8_t)(BLUE_G * blueFactor + WHITE_G * whiteFactor);
          uint8_t b = (uint8_t)(BLUE_B * blueFactor + WHITE_B * whiteFactor);
          
          // Appliquer la luminosité globale
          r = (r * currentBrightness) / 255;
          g = (g * currentBrightness) / 255;
          b = (b * currentBrightness) / 255;
          
          uint32_t color = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
          strip->setPixelColor(i, color);
        }
      }
      break;
    }
    
    case LED_EFFECT_BREATHE: {
      // Effet de respiration avec changement de couleur toutes les 30 secondes
      static unsigned long breatheStartTime = 0;
      static int currentColorIndex = 0;
      static unsigned long colorChangeStartTime = 0;
      static uint8_t previousR = 30, previousG = 100, previousB = 255;  // Couleur précédente pour transition
      
      // Palette de couleurs pour la respiration (définie en premier pour être accessible partout)
      // Couleurs douces et apaisantes
      const uint8_t colors[][3] = {
        {30, 100, 255},   // Bleu doux
        {100, 150, 255},  // Bleu ciel
        {150, 100, 255},  // Violet doux
        {255, 100, 150},  // Rose doux
        {255, 150, 100},  // Orange doux
        {150, 255, 150},  // Vert doux
        {255, 200, 100}   // Jaune doux
      };
      const int numColors = sizeof(colors) / sizeof(colors[0]);
      
      // Initialiser le temps de départ si nécessaire
      if (breatheStartTime == 0) {
        breatheStartTime = currentTime;
        colorChangeStartTime = currentTime;
        // Initialiser avec la première couleur
        previousR = colors[0][0];
        previousG = colors[0][1];
        previousB = colors[0][2];
      }
      
      // Cycle de changement de couleur : 30 secondes
      const uint32_t COLOR_CHANGE_INTERVAL_MS = 30000;  // 30 secondes
      const uint32_t COLOR_TRANSITION_DURATION_MS = 2000;  // 2 secondes pour la transition
      
      // Gérer le wrap-around de millis()
      uint32_t elapsed;
      if (currentTime >= breatheStartTime) {
        elapsed = currentTime - breatheStartTime;
      } else {
        // Wrap-around détecté
        elapsed = ULONG_MAX - breatheStartTime + currentTime;
      }
      
      // Changer de couleur toutes les 30 secondes
      int newColorIndex = elapsed / COLOR_CHANGE_INTERVAL_MS;
      if (newColorIndex != currentColorIndex) {
        // Sauvegarder la couleur actuelle comme couleur précédente
        previousR = colors[currentColorIndex % numColors][0];
        previousG = colors[currentColorIndex % numColors][1];
        previousB = colors[currentColorIndex % numColors][2];
        currentColorIndex = newColorIndex;
        colorChangeStartTime = currentTime;
      }
      
      // Sélectionner la couleur cible (cycle infini)
      uint8_t targetR = colors[currentColorIndex % numColors][0];
      uint8_t targetG = colors[currentColorIndex % numColors][1];
      uint8_t targetB = colors[currentColorIndex % numColors][2];
      
      // Calculer la transition progressive entre l'ancienne et la nouvelle couleur
      uint32_t transitionElapsed;
      if (currentTime >= colorChangeStartTime) {
        transitionElapsed = currentTime - colorChangeStartTime;
      } else {
        transitionElapsed = ULONG_MAX - colorChangeStartTime + currentTime;
      }
      
      uint8_t currentR, currentG, currentB;
      if (transitionElapsed < COLOR_TRANSITION_DURATION_MS) {
        // Transition en cours : mélanger progressivement les couleurs
        float transitionFactor = (float)transitionElapsed / (float)COLOR_TRANSITION_DURATION_MS;
        // Utiliser une courbe d'ease-in-out pour une transition plus douce
        float easedFactor = transitionFactor * transitionFactor * (3.0 - 2.0 * transitionFactor);
        
        currentR = (uint8_t)(previousR + (targetR - previousR) * easedFactor);
        currentG = (uint8_t)(previousG + (targetG - previousG) * easedFactor);
        currentB = (uint8_t)(previousB + (targetB - previousB) * easedFactor);
      } else {
        // Transition terminée : utiliser la couleur cible
        currentR = targetR;
        currentG = targetG;
        currentB = targetB;
      }
      
      // Cycle de respiration : ~3 secondes pour un cycle complet (inspiration + expiration)
      const uint32_t BREATHE_CYCLE_MS = 3000;  // 3 secondes
      uint32_t breatheElapsed = elapsed % BREATHE_CYCLE_MS;
      
      // Créer l'effet de respiration avec une fonction sinusoïdale
      // sin va de -1 à 1, on le transforme en 0.3 à 1.0 pour avoir un minimum de luminosité
      float breatheFactor = sin((float)breatheElapsed / (float)BREATHE_CYCLE_MS * 2.0 * M_PI) * 0.35 + 0.65;
      // breatheFactor va maintenant de 0.3 à 1.0
      
      if (strip != nullptr) {
        // Calculer les composantes RGB avec l'effet de respiration
        // Utiliser currentR/G/B qui contient la couleur avec transition progressive
        uint8_t r = (uint8_t)(currentR * breatheFactor);
        uint8_t g = (uint8_t)(currentG * breatheFactor);
        uint8_t b = (uint8_t)(currentB * breatheFactor);
        
        // Appliquer la luminosité globale
        r = (r * currentBrightness) / 255;
        g = (g * currentBrightness) / 255;
        b = (b * currentBrightness) / 255;
        
        uint32_t color = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        
        // Appliquer la couleur à toutes les LEDs
        for (int i = 0; i < NUM_LEDS; i++) {
          strip->setPixelColor(i, color);
        }
      }
      break;
    }
  }
}
