/**
 * Gestionnaire du touch Dream - plus fluide pour un enfant
 * - Double tap : démarre ou arrête routine coucher/réveil
 * - Appui maintenu 1s+ (sans relâcher) : envoi notification alerte
 */

#include "dream_touch_handler.h"
#include "../bedtime/bedtime_manager.h"
#include "../wakeup/wakeup_manager.h"
#include "../../config/dream_config.h"
#include "../../api/dream_api_routes.h"
#include "../../../../common/managers/touch/touch_manager.h"
#include "../../../../common/managers/led/led_manager.h"
#include "../../../../color/colors.h"

static const unsigned long TAP_MAX_MS = 600;            // Max pour un tap (enfant = appui un peu plus long)
static const unsigned long HOLD_ALERT_MS = 1000;        // Appui 1s+ = envoi alerte (sans relâcher)
static const unsigned long DOUBLE_TAP_WINDOW_MS = 1000; // Fenêtre entre 2 taps (1s pour plus de fluidité)

void DreamTouchHandler::update() {
  if (!TouchManager::isInitialized()) return;

  static bool dreamTouchLast = false;
  static unsigned long dreamTouchStartMs = 0;
  static unsigned long lastTapEndMs = 0;
  static bool alertHoldFired = false;
  static unsigned long noRoutineFeedbackUntil = 0;
  static unsigned long alertFeedbackUntil = 0;

  bool touched = TouchManager::isTouched();
  unsigned long now = millis();

  if (noRoutineFeedbackUntil > 0 && now >= noRoutineFeedbackUntil) {
    noRoutineFeedbackUntil = 0;
    LEDManager::startFadeOutAndClear();
  }
  if (alertFeedbackUntil > 0 && now >= alertFeedbackUntil) {
    alertFeedbackUntil = 0;
    if (BedtimeManager::isBedtimeActive()) BedtimeManager::restoreDisplayFromConfig();
    else if (!WakeupManager::isWakeupActive()) LEDManager::startFadeOutAndClear();
  }

  if (touched && !dreamTouchLast) {
    dreamTouchStartMs = now;
    alertHoldFired = false;
  }

  unsigned long duration = touched ? (now - dreamTouchStartMs) : 0;

  // Appui maintenu 1s+ (sans relâcher) : envoi alerte immédiatement
  if (touched && duration >= HOLD_ALERT_MS && !alertHoldFired) {
    alertHoldFired = true;
    DreamConfig dreamConfig = DreamConfigManager::getConfig();
    if (dreamConfig.nighttime_alert_enabled) {
#ifdef HAS_WIFI
      DreamApiRoutes::postNighttimeAlert();
#endif
      if (Serial) Serial.println("[DREAM] Appui 1s: alerte envoyee");
      LEDManager::preventSleep();
      LEDManager::wakeUp();
      LEDManager::setColor(COLOR_GREEN);
      LEDManager::setEffect(LED_EFFECT_PULSE_FAST);
      alertFeedbackUntil = now + 2000;
    }
  }

  // Relâchement
  if (!touched && dreamTouchLast) {
    unsigned long releaseDuration = now - dreamTouchStartMs;

    // Tap court (80-600ms) : double tap = routine coucher/réveil
    if (releaseDuration >= 80 && releaseDuration <= TAP_MAX_MS) {
      if (lastTapEndMs > 0 && (now - lastTapEndMs) <= DOUBLE_TAP_WINDOW_MS) {
        lastTapEndMs = 0;
        if (BedtimeManager::isBedtimeActive()) {
          BedtimeManager::stopBedtimeManually();
          if (Serial) Serial.println("[DREAM] Double tap: routine coucher arretee");
        } else if (WakeupManager::isWakeupActive()) {
          WakeupManager::stopWakeupManually();
          if (Serial) Serial.println("[DREAM] Double tap: routine reveil arretee");
        } else {
          if (BedtimeManager::isBedtimeEnabled()) {
            BedtimeManager::startBedtimeManually();
            if (Serial) Serial.println("[DREAM] Double tap: routine coucher lancee");
          } else {
            LEDManager::preventSleep();
            LEDManager::wakeUp();
            LEDManager::setColor(COLOR_RED);
            LEDManager::setEffect(LED_EFFECT_PULSE_FAST);
            noRoutineFeedbackUntil = now + 3000;
            if (Serial) Serial.println("[DREAM] Double tap: pas de routine pour aujourd'hui");
          }
        }
      } else {
        lastTapEndMs = now;
      }
    }
    else {
      lastTapEndMs = 0;
    }
  }
  dreamTouchLast = touched;
}
