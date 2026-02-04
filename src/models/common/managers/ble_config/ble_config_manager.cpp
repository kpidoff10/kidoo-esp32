#include "ble_config_manager.h"
#include "../ble/ble_manager.h"
#include "../led/led_manager.h"
#include "../../../model_config.h"

// Variables statiques
bool BLEConfigManager::initialized = false;
uint8_t BLEConfigManager::buttonPin = 0;
BLEConfigManager::ButtonState BLEConfigManager::buttonState = BUTTON_IDLE;
unsigned long BLEConfigManager::pressStartTime = 0;
unsigned long BLEConfigManager::bleEnableTime = 0;
uint32_t BLEConfigManager::bleDuration = 0;
uint32_t BLEConfigManager::defaultDuration = DEFAULT_BLE_DURATION;
uint32_t BLEConfigManager::longPressDuration = DEFAULT_LONG_PRESS;
bool BLEConfigManager::bleEnabled = false;
bool BLEConfigManager::feedbackActive = false;
bool BLEConfigManager::feedbackEnabled = false;  // Indique si le feedback était activé au départ
unsigned long BLEConfigManager::lastFeedbackTime = 0;
unsigned long BLEConfigManager::buttonCooldownUntil = 0;

bool BLEConfigManager::init(uint8_t buttonPin) {
  if (initialized) {
    return true;
  }
  
  BLEConfigManager::buttonPin = buttonPin;
  
  // Configurer le pin en INPUT_PULLUP (bouton connecté à GND)
  pinMode(buttonPin, INPUT_PULLUP);
  
  initialized = true;
  bleEnabled = false;
  buttonState = BUTTON_IDLE;
  buttonCooldownUntil = 0;
  
  Serial.println("[BLE-CONFIG] Gestionnaire d'activation BLE initialise");
  Serial.print("[BLE-CONFIG] Pin bouton: GPIO ");
  Serial.println(buttonPin);
  Serial.print("[BLE-CONFIG] Appui long requis: ");
  Serial.print(longPressDuration);
  Serial.println(" ms");
  Serial.print("[BLE-CONFIG] Duree d'activation: ");
  Serial.print(defaultDuration / 1000);
  Serial.println(" secondes");
  Serial.println("[BLE-CONFIG] BLE desactive par defaut (appui long pour activer)");
  
  return true;
}

bool BLEConfigManager::isInitialized() {
  return initialized;
}

void BLEConfigManager::update() {
  if (!initialized) {
    return;
  }
  
  // Gérer la détection du bouton
  handleButtonPress();
  
  // Gérer le timeout du BLE
  if (bleEnabled) {
    unsigned long currentTime = millis();
    // Gérer le wrap-around de millis() (tous les ~49 jours)
    unsigned long elapsed;
    if (currentTime >= bleEnableTime) {
      // Pas de wrap-around
      elapsed = currentTime - bleEnableTime;
    } else {
      // Wrap-around détecté, calculer correctement
      elapsed = (ULONG_MAX - bleEnableTime) + currentTime + 1;
    }
    
    if (elapsed >= bleDuration) {
      // Timeout atteint : purge complète pour libérer la RAM
      handleBLEDeactivation(true);
    } else {
      // Vérifier si un client BLE est connecté
      #ifdef HAS_BLE
      bool connected = BLEManager::isConnected();
      
      // BLE activé = respiration bleue en permanence (connecté ou non). On ne coupe jamais le feedback.
      if (!connected && !feedbackActive && bleEnabled && feedbackEnabled) {
        // Le client s'est déconnecté, réactiver le feedback lumineux (PULSE bleu)
        // Seulement si le feedback était activé au départ
        // IMPORTANT: Faire un clear() d'abord pour éliminer toute couleur résiduelle
        #ifdef HAS_LED
        if (HAS_LED) {
          LEDManager::clear();  // Éteindre d'abord pour éliminer les couleurs résiduelles
          delay(150);  // Attendre que clear() soit traité par le thread LED
          LEDManager::setColor(0, 0, 255);  // Bleu
          delay(150);  // Attendre que la couleur bleue soit bien définie avant d'activer PULSE
          LEDManager::setEffect(LED_EFFECT_PULSE);  // Respiration
        }
        #endif
        feedbackActive = true;
        Serial.println("[BLE-CONFIG] Client deconnecte - Feedback lumineux reactive");
      }
      #endif
      
      // Mettre à jour le feedback visuel (seulement si pas connecté)
      if (!connected) {
        updateFeedback();
      }
    }
  }
}

bool BLEConfigManager::isBLEEnabled() {
  return initialized && bleEnabled;
}

bool BLEConfigManager::enableBLE(uint32_t durationMs, bool enableFeedback) {
  if (!initialized) {
    return false;
  }
  
  if (durationMs == 0) {
    durationMs = defaultDuration;
  }
  
  bleDuration = durationMs;
  bleEnableTime = millis();
  bleEnabled = true;
  feedbackEnabled = enableFeedback;  // Mémoriser si le feedback était activé au départ
  feedbackActive = enableFeedback;  // Contrôler le feedback selon le paramètre
  lastFeedbackTime = millis();
  
  // Activer le BLE si disponible (ré-init si purge précédente après timeout)
  #ifdef HAS_BLE
  if (HAS_BLE) {
    if (!BLEManager::isInitialized() && BLEManager::getDeviceNameForReinit() != nullptr) {
      if (!BLEManager::init(BLEManager::getDeviceNameForReinit())) {
        Serial.println("[BLE-CONFIG] WARNING: re-init BLE apres purge a echoue");
        bleEnabled = false;
        return false;
      }
      Serial.println("[BLE-CONFIG] BLE re-initialise apres purge");
    }
    if (BLEManager::isInitialized() && BLEManager::isAvailable()) {
      BLEManager::startAdvertising();
      if (enableFeedback) {
        Serial.println("[BLE-CONFIG] BLE active via bouton");
      } else {
        Serial.println("[BLE-CONFIG] BLE active automatiquement (sans feedback lumineux)");
      }
    } else {
      Serial.println("[BLE-CONFIG] WARNING: BLE non disponible, activation impossible");
      bleEnabled = false;
      return false;
    }
  }
  #else
  Serial.println("[BLE-CONFIG] WARNING: BLE non disponible sur ce modele");
  bleEnabled = false;
  return false;
  #endif
  
  Serial.print("[BLE-CONFIG] Duree d'activation: ");
  Serial.print(durationMs / 1000);
  Serial.println(" secondes");
  
  // Feedback lumineux : BLE activé = respiration bleue en permanence (connecté ou non)
  // Les LEDs sont allumées dès l'activation du BLE et restent en respiration bleue tout le temps
  if (enableFeedback) {
    #ifdef HAS_LED
    if (HAS_LED) {
      LEDManager::clear();  // Éteindre d'abord pour éliminer les couleurs résiduelles (ex: orange de l'init)
      delay(150);  // Attendre que clear() soit traité par le thread LED
      LEDManager::setColor(0, 0, 255);  // Bleu
      delay(150);  // Attendre que la couleur bleue soit bien définie avant d'activer PULSE
      LEDManager::setEffect(LED_EFFECT_PULSE);  // Respiration bleue tant que BLE actif
    }
    #endif
  } else {
    // BLE activé sans feedback (ex: auto car pas de WiFi) -> ne pas toucher aux LEDs
    // IMPORTANT: Si les LEDs sont déjà en sleep mode, ne pas les réveiller avec des commandes
    #ifdef HAS_LED
    if (HAS_LED && !LEDManager::getSleepState()) {
      // Les LEDs ne sont pas en sleep, on peut les éteindre proprement
      LEDManager::setEffect(LED_EFFECT_NONE);
      LEDManager::setColor(0, 0, 0);
      LEDManager::clear();
      Serial.println("[BLE-CONFIG] LEDs eteintes (pas en sleep mode)");
    } else if (HAS_LED) {
      // Les LEDs sont en sleep mode, ne rien faire pour éviter de les réveiller
      Serial.println("[BLE-CONFIG] LEDs en sleep mode - pas de commande LED envoyee");
    }
    #endif
  }
  
  return true;
}

void BLEConfigManager::disableBLE() {
  if (!initialized || !bleEnabled) {
    return;
  }
  // Désactivation douce (WiFi connecté) : pas de purge pour éviter crash avec la tâche BLE
  handleBLEDeactivation(false);
}

uint32_t BLEConfigManager::getRemainingTime() {
  if (!initialized || !bleEnabled) {
    return 0;
  }
  
  unsigned long currentTime = millis();
  // Gérer le wrap-around de millis() (tous les ~49 jours)
  unsigned long elapsed;
  if (currentTime >= bleEnableTime) {
    // Pas de wrap-around
    elapsed = currentTime - bleEnableTime;
  } else {
    // Wrap-around détecté, calculer correctement
    elapsed = (ULONG_MAX - bleEnableTime) + currentTime + 1;
  }
  
  if (elapsed >= bleDuration) {
    return 0;
  }
  
  return bleDuration - elapsed;
}

void BLEConfigManager::setDefaultDuration(uint32_t durationMs) {
  defaultDuration = durationMs;
  Serial.print("[BLE-CONFIG] Duree par defaut modifiee: ");
  Serial.print(durationMs / 1000);
  Serial.println(" secondes");
}

void BLEConfigManager::setLongPressDuration(uint32_t durationMs) {
  longPressDuration = durationMs;
  Serial.print("[BLE-CONFIG] Duree d'appui long modifiee: ");
  Serial.print(durationMs);
  Serial.println(" ms");
}

void BLEConfigManager::printInfo() {
  if (!initialized) {
    Serial.println("[BLE-CONFIG] Non initialise");
    return;
  }
  
  Serial.println("");
  Serial.println("========== BLE Config Manager ==========");
  Serial.print("Pin bouton: GPIO ");
  Serial.println(buttonPin);
  Serial.print("Appui long requis: ");
  Serial.print(longPressDuration);
  Serial.println(" ms");
  Serial.print("Duree par defaut: ");
  Serial.print(defaultDuration / 1000);
  Serial.println(" secondes");
  Serial.print("BLE active: ");
  Serial.println(bleEnabled ? "OUI" : "NON");
  
  if (bleEnabled) {
    uint32_t remaining = getRemainingTime();
    Serial.print("Temps restant: ");
    Serial.print(remaining / 1000);
    Serial.println(" secondes");
  }
  
  Serial.println("=========================================");
}

// ============================================
// Méthodes privées
// ============================================

void BLEConfigManager::handleButtonPress() {
  bool pressed = isButtonPressed();
  unsigned long currentTime = millis();
  
  switch (buttonState) {
    case BUTTON_IDLE:
      if (pressed) {
        // Début d'appui détecté
        buttonState = BUTTON_PRESSED;
        pressStartTime = currentTime;
        Serial.println("[BLE-CONFIG] Appui detecte...");
      }
      break;
      
    case BUTTON_PRESSED:
      if (!pressed) {
        // Bouton relâché avant le seuil
        buttonState = BUTTON_IDLE;
        // Activer une période de refroidissement pour éviter les détections multiples
        unsigned long currentTime = millis();
        buttonCooldownUntil = currentTime + COOLDOWN_DELAY;
        Serial.println("[BLE-CONFIG] Appui annule (trop court)");
      } else {
        // Vérifier si on a atteint 10 secondes -> reboot
        unsigned long pressDuration = currentTime - pressStartTime;
        if (pressDuration >= REBOOT_LONG_PRESS_MS) {
          if (Serial) {
            Serial.println("[BLE-CONFIG] Appui 10s detecte -> REBOOT");
          }
          ESP.restart();
        }
        // Vérifier si on a atteint le seuil d'appui long (3s) -> BLE
        if (pressDuration >= longPressDuration) {
          // Appui long détecté !
          buttonState = BUTTON_LONG_PRESS;
          handleBLEActivation();
        }
        // Pas de feedback visuel pendant l'appui - le bleu apparaîtra uniquement après activation
      }
      break;
      
    case BUTTON_LONG_PRESS:
      if (!pressed) {
        // Bouton relâché après activation
        buttonState = BUTTON_IDLE;
      } else {
        // Vérifier si on tient toujours 10 secondes au total -> reboot
        unsigned long pressDuration = currentTime - pressStartTime;
        if (pressDuration >= REBOOT_LONG_PRESS_MS) {
          if (Serial) {
            Serial.println("[BLE-CONFIG] Appui 10s detecte -> REBOOT");
          }
          ESP.restart();
        }
      }
      break;
      
    case BUTTON_RELEASED:
      buttonState = BUTTON_IDLE;
      break;
  }
}

void BLEConfigManager::handleBLEActivation() {
  Serial.println("");
  Serial.println("[BLE-CONFIG] ========================================");
  Serial.println("[BLE-CONFIG] APPUI LONG DETECTE - Activation BLE");
  Serial.println("[BLE-CONFIG] ========================================");
  
  // Activer le BLE avec feedback lumineux (appui bouton)
  if (enableBLE(0, true)) {
    // Le feedback lumineux est déjà géré dans enableBLE() si enableFeedback = true
    // Pas besoin de le refaire ici
  } else {
    // Erreur d'activation - feedback d'erreur
    #ifdef HAS_LED
    if (HAS_LED) {
      LEDManager::setColor(255, 0, 0);  // Rouge (erreur)
      delay(500);
      LEDManager::setColor(0, 0, 0);    // Éteint
    }
    #endif
  }
}

void BLEConfigManager::handleBLEDeactivation(bool fullShutdown) {
  if (!bleEnabled) {
    return;
  }
  
  bleEnabled = false;
  feedbackActive = false;
  feedbackEnabled = false;
  
  Serial.println("");
  Serial.println("[BLE-CONFIG] ========================================");
  Serial.println(fullShutdown ? "[BLE-CONFIG] Desactivation BLE (timeout)" : "[BLE-CONFIG] Desactivation BLE (WiFi connecte)");
  Serial.println("[BLE-CONFIG] ========================================");
  
  #ifdef HAS_BLE
  if (HAS_BLE && BLEManager::isInitialized()) {
    if (fullShutdown) {
      // Timeout : purge complète pour libérer la RAM (task, queue, deinit)
      BLEManager::shutdownForOta();
      Serial.println("[BLE-CONFIG] BLE purge (mem liberee)");
    } else {
      // WiFi connecté : arrêt advertising seulement (évite crash si tâche BLE encore active)
      BLEManager::stopAdvertising();
    }
  }
  #endif
  
  // Feedback visuel : LED éteinte
  #ifdef HAS_LED
  if (HAS_LED) {
    LEDManager::setEffect(LED_EFFECT_NONE);
    LEDManager::setColor(0, 0, 0);
    LEDManager::clear();
  }
  #endif
}

void BLEConfigManager::updateFeedback() {
  if (!feedbackActive) {
    return;
  }
  
  // Le feedback est géré par l'effet PULSE du LEDManager
  // Pas besoin de clignotement manuel - l'effet de respiration est continu
  // Le bleu reste actif avec effet de respiration tant que le BLE est en mode appairage
  #ifdef HAS_LED
  if (HAS_LED) {
    // S'assurer que le bleu avec effet de respiration est toujours actif
    // LEDManager gère automatiquement l'effet PULSE
    // Pas besoin de mettre à jour ici, l'effet est déjà configuré dans enableBLE()
  }
  #endif
}

bool BLEConfigManager::isButtonPressed() {
  // Bouton en INPUT_PULLUP : LOW = pressé, HIGH = relâché
  // Anti-rebond amélioré avec période de refroidissement
  static unsigned long lastDebounceTime = 0;
  static bool lastButtonState = HIGH;
  static bool debouncedState = HIGH;
  
  unsigned long currentTime = millis();
  bool reading = digitalRead(buttonPin);
  
  // Vérifier si on est en période de refroidissement
  if (buttonCooldownUntil > 0) {
    unsigned long cooldownElapsed;
    if (currentTime >= buttonCooldownUntil) {
      // Période de refroidissement terminée
      buttonCooldownUntil = 0;
    } else {
      // Encore en période de refroidissement, ignorer les changements
      return false;
    }
  }
  
  // Gérer le wrap-around de millis()
  unsigned long debounceElapsed;
  if (currentTime >= lastDebounceTime) {
    debounceElapsed = currentTime - lastDebounceTime;
  } else {
    debounceElapsed = (ULONG_MAX - lastDebounceTime) + currentTime + 1;
  }
  
  // Si l'état a changé, réinitialiser le timer de debounce
  if (reading != lastButtonState) {
    lastDebounceTime = currentTime;
  }
  
  // Si le délai de debounce est écoulé, mettre à jour l'état débouncé
  if (debounceElapsed > DEBOUNCE_DELAY) {
    if (reading != debouncedState) {
      debouncedState = reading;
    }
  }
  
  lastButtonState = reading;
  
  // Retourner true si le bouton est pressé (LOW) et stable
  return (debouncedState == LOW);
}
