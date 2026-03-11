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
#include "../../pubnub/model_pubnub_routes.h"
#include "../../utils/led_effect_parser.h"
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

/** Parse l'effet par défaut depuis la config (string → LEDEffect enum) */
static LEDEffect parseDefaultEffect(const char* effectStr) {
  // Utiliser LEDEffectParser pour la conversion uniforme
  return LEDEffectParser::parse(effectStr);
}

static const unsigned long HOLD_ALERT_MS = 2000; // Appui 2s+ = envoi alerte (sans relâcher)
static const unsigned long ALERT_FEEDBACK_MS = 3000; // Durée du feedback alerte avant reprise du mode

unsigned long DreamTouchHandler::s_alertFeedbackUntil = 0;

// Variable statique exposée au reste du système
static bool s_defaultColorDisplayed = false;  // Track if default color is currently on

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
    } else if (s_defaultColorDisplayed) {
      // Reprendre la couleur par défaut après l'alerte
      DreamConfig dreamConfig = DreamConfigManager::getConfig();
      LEDManager::preventSleep();
      LEDManager::wakeUp();
      // Nettoyer d'abord pour éviter les animations résiduelles du pulse
      LEDManager::clear();
      LEDManager::setColor(dreamConfig.default_color_r, dreamConfig.default_color_g, dreamConfig.default_color_b);
      LEDManager::setBrightness(LEDManager::brightnessPercentTo255(dreamConfig.default_brightness));
      LEDEffect defaultEffect = LEDEffectParser::parse(dreamConfig.default_effect);
      LEDManager::setEffect(defaultEffect);
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
      // Effet avant couleur pour pulse direct (évite flash noir)
      LEDManager::setEffect(LED_EFFECT_PULSE_FAST);
      if (ok) {
        LEDManager::setColor(COLOR_GREEN);  // Vert
      } else {
        LEDManager::setColor(COLOR_RED);  // Rouge
      }
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
          // Toggle default color display on/off
          if (s_defaultColorDisplayed) {
            // Default color is on → turn it off
            LEDManager::clear();
            s_defaultColorDisplayed = false;
            ModelDreamPubNubRoutes::publishDefaultColorState();
            if (Serial) Serial.println("[DREAM] Tap: couleur par defaut eteinte");
          } else {
            // Default color is off → turn it on
            DreamConfig dreamConfig = DreamConfigManager::getConfig();
            LEDManager::preventSleep();
            LEDManager::wakeUp();
            LEDManager::setColor(dreamConfig.default_color_r, dreamConfig.default_color_g, dreamConfig.default_color_b);
            LEDManager::setBrightness(LEDManager::brightnessPercentTo255(dreamConfig.default_brightness));
            LEDEffect defaultEffect = LEDEffectParser::parse(dreamConfig.default_effect);
            LEDManager::setEffect(defaultEffect);
            s_defaultColorDisplayed = true;
            ModelDreamPubNubRoutes::publishDefaultColorState();
            if (Serial) Serial.printf("[DREAM] Tap: couleur par defaut affichee (RGB:%d,%d,%d, effet:%s)\n",
                                       dreamConfig.default_color_r, dreamConfig.default_color_g,
                                       dreamConfig.default_color_b, dreamConfig.default_effect);
          }
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
  // Vert si success, rouge sinon
  if (success) {
    LEDManager::setColor(COLOR_GREEN);  // Vert
  } else {
    LEDManager::setColor(COLOR_RED);  // Rouge
  }
  LEDManager::setEffect(LED_EFFECT_PULSE_FAST);
  s_alertFeedbackUntil = millis() + ALERT_FEEDBACK_MS;
}

bool DreamTouchHandler::isDefaultColorDisplayed() {
  return s_defaultColorDisplayed;
}
