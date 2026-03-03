/**
 * Gestionnaire du touch Dream - plus fluide pour un enfant
 * - Tout touche (50ms à 2s) : démarre ou arrête routine coucher/réveil au relâchement
 * - Appui maintenu 2s+ (sans relâcher) : envoi notification alerte
 */

#include "dream_touch_handler.h"
#include "../bedtime/bedtime_manager.h"
#include "../wakeup/wakeup_manager.h"
#include "../../config/dream_config.h"
#include "../../api/dream_api_routes.h"
#include "../../../../common/managers/touch/touch_manager.h"
#include "../../../../common/managers/led/led_manager.h"
#include "../../../../common/managers/sd/sd_manager.h"
#include "../../../../color/colors.h"

/** Récupère la luminosité de la config (0-255) selon le mode actif */
static uint8_t getBrightnessFromConfig() {
  if (BedtimeManager::isBedtimeActive()) {
    return LEDManager::brightnessPercentTo255(BedtimeManager::getConfig().brightness);
  }
  if (WakeupManager::isWakeupActive()) {
    return LEDManager::brightnessPercentTo255(WakeupManager::getConfig().brightness);
  }
  return LEDManager::brightnessPercentTo255(SDManager::getConfig().bedtime_brightness);
}

static const unsigned long HOLD_ALERT_MS = 2000; // Appui 2s+ = envoi alerte (sans relâcher)
static const unsigned long ALERT_FEEDBACK_MS = 3000; // Durée du feedback alerte avant reprise du mode

unsigned long DreamTouchHandler::s_alertFeedbackUntil = 0;

void DreamTouchHandler::update() {
  if (!TouchManager::isInitialized()) return;

  static bool dreamTouchLast = false;
  static unsigned long dreamTouchStartMs = 0;
  static bool alertHoldFired = false;
  static unsigned long noRoutineFeedbackUntil = 0;

  bool touched = TouchManager::isTouched();
  unsigned long now = millis();

  if (noRoutineFeedbackUntil > 0 && now >= noRoutineFeedbackUntil) {
    noRoutineFeedbackUntil = 0;
    LEDManager::startFadeOutAndClear();
  }
  if (s_alertFeedbackUntil > 0 && now >= s_alertFeedbackUntil) {
    s_alertFeedbackUntil = 0;
    if (BedtimeManager::isBedtimeActive()) {
      BedtimeManager::restoreDisplayFromConfig();
    } else if (WakeupManager::isWakeupActive()) {
      WakeupConfig wc = WakeupManager::getConfig();
      uint8_t brightnessValue = LEDManager::brightnessPercentTo255(wc.brightness);
      LEDManager::preventSleep();
      LEDManager::wakeUp();
      LEDManager::setEffect(LED_EFFECT_NONE);
      LEDManager::setColor(wc.colorR, wc.colorG, wc.colorB);
      LEDManager::setBrightness(brightnessValue);
    } else {
      LEDManager::startFadeOutAndClear();
    }
  }

  if (touched && !dreamTouchLast) {
    dreamTouchStartMs = now;
    alertHoldFired = false;
  }

  unsigned long duration = touched ? (now - dreamTouchStartMs) : 0;

  // Appui maintenu 2s+ (sans relâcher) : envoi alerte immédiatement
  if (touched && duration >= HOLD_ALERT_MS && !alertHoldFired) {
    alertHoldFired = true;
    DreamConfig dreamConfig = DreamConfigManager::getConfig();
    if (dreamConfig.nighttime_alert_enabled) {
#ifdef HAS_WIFI
      LEDManager::preventSleep();
      LEDManager::wakeUp();
      LEDManager::setBrightness(getBrightnessFromConfig());
      s_alertFeedbackUntil = now + ALERT_FEEDBACK_MS;
      bool ok = DreamApiRoutes::postNighttimeAlert();
      // Nettoyer d'abord pour éviter les couleurs résiduelles
      LEDManager::clear();
      delay(50);
      // Vert si succès, rouge si erreur
      if (ok) {
        LEDManager::setColor(COLOR_GREEN);  // Vert
      } else {
        LEDManager::setColor(COLOR_RED);  // Rouge
      }
      delay(50);  // Laisser la couleur bien définie avant l'effet
      LEDManager::setEffect(LED_EFFECT_PULSE_FAST);  // Pulse avec la bonne couleur
      if (ok) delay(100);
      if (Serial) Serial.printf("[DREAM] Appui 2s: alerte %s\n", ok ? "envoyee" : "echec");
#else
      if (Serial) Serial.println("[DREAM] Appui 2s: alerte (WiFi non dispo)");
#endif
    }
  }

  // Relâchement
  if (!touched && dreamTouchLast) {
    unsigned long releaseDuration = now - dreamTouchStartMs;

    // Tout touche < 2s : démarre ou arrête routine coucher/réveil au relâchement immédiatement
    // (appui 2s+ = alerte déjà gérée pendant l'appui, pas au relâchement)
    if (releaseDuration < HOLD_ALERT_MS && !alertHoldFired) {
      if (BedtimeManager::isBedtimeActive()) {
        BedtimeManager::stopBedtimeManually();
        if (Serial) Serial.println("[DREAM] Tap: routine coucher arretee");
      } else if (WakeupManager::isWakeupActive()) {
        WakeupManager::stopWakeupManually();
        if (Serial) Serial.println("[DREAM] Tap: routine reveil arretee");
      } else {
        if (BedtimeManager::isBedtimeEnabled()) {
          BedtimeManager::startBedtimeManually();
          if (Serial) Serial.println("[DREAM] Tap: routine coucher lancee");
        } else {
          LEDManager::preventSleep();
          LEDManager::wakeUp();
          LEDManager::setColor(255, 0, 0);  // Rouge
          LEDManager::setEffect(LED_EFFECT_PULSE_FAST);
          noRoutineFeedbackUntil = now + 3000;
          if (Serial) Serial.println("[DREAM] Tap: pas de routine pour aujourd'hui");
        }
      }
    }
  }
  dreamTouchLast = touched;
}

void DreamTouchHandler::triggerAlertFeedback(bool success) {
  LEDManager::preventSleep();
  LEDManager::wakeUp();
  LEDManager::setBrightness(getBrightnessFromConfig());
  // Nettoyer d'abord pour éviter les couleurs résiduelles
  LEDManager::clear();
  delay(50);
  // Vert si success, rouge sinon
  if (success) {
    LEDManager::setColor(COLOR_GREEN);  // Vert
  } else {
    LEDManager::setColor(COLOR_RED);  // Rouge
  }
  delay(50);  // Laisser la couleur bien définie avant l'effet
  LEDManager::setEffect(LED_EFFECT_PULSE_FAST);
  s_alertFeedbackUntil = millis() + ALERT_FEEDBACK_MS;
}
