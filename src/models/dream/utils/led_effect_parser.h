#ifndef LED_EFFECT_PARSER_H
#define LED_EFFECT_PARSER_H

#include <Arduino.h>
#include "common/managers/led/led_manager.h"

/**
 * Parser pour convertir les noms d'effets LED (strings) en énumérations LEDEffect
 * Consolide les conversions présentes dans:
 * - BedtimeManager::startBedtime()
 * - BedtimeManager::updateFadeOut()
 * - DreamTouchHandler::triggerAlertFeedback()
 * - LEDEffectParser::parse() pour effet par défaut (config, touch, PubNub)
 */
class LEDEffectParser {
public:
  /**
   * Convertir un nom d'effet (string) en énumération LEDEffect
   *
   * Effets supportés:
   * - "none", "solid" → LED_EFFECT_NONE (couleur fixe)
   * - "pulse_fast" → LED_EFFECT_PULSE_FAST
   * - "pulse" → LED_EFFECT_PULSE
   * - "rainbow", "rainbow_soft" ou "rainbow-soft" → LED_EFFECT_RAINBOW_SOFT
   * - "rotate" → LED_EFFECT_ROTATE
   * - "breathe" → LED_EFFECT_BREATHE
   * - "nightlight" → LED_EFFECT_NIGHTLIGHT
   * - "glossy" → LED_EFFECT_GLOSSY (si disponible)
   * - String vide → LED_EFFECT_NONE
   * - Inconnu → LED_EFFECT_NONE
   *
   * @param effectStr Nom de l'effet
   * @return LEDEffect correspondant ou LED_EFFECT_NONE si inconnu
   */
  static LEDEffect parse(const char* effectStr) {
    if (!effectStr || effectStr[0] == '\0') {
      return LED_EFFECT_NONE;  // Empty string = solid color
    }

    // Handle common aliases for "none"
    if (strcmp(effectStr, "none") == 0 || strcmp(effectStr, "solid") == 0) {
      return LED_EFFECT_NONE;
    }

    // Check all effect types
    if (strcmp(effectStr, "pulse_fast") == 0) {
      return LED_EFFECT_PULSE_FAST;
    }
    if (strcmp(effectStr, "pulse") == 0) {
      return LED_EFFECT_PULSE;
    }
    if (strcmp(effectStr, "rainbow") == 0) {
      return LED_EFFECT_RAINBOW;
    }
    if (strcmp(effectStr, "rainbow_soft") == 0 || strcmp(effectStr, "rainbow-soft") == 0) {
      return LED_EFFECT_RAINBOW_SOFT;
    }
    if (strcmp(effectStr, "rotate") == 0) {
      return LED_EFFECT_ROTATE;
    }
    if (strcmp(effectStr, "breathe") == 0) {
      return LED_EFFECT_BREATHE;
    }
    if (strcmp(effectStr, "nightlight") == 0) {
      return LED_EFFECT_NIGHTLIGHT;
    }
    if (strcmp(effectStr, "glossy") == 0) {
      return LED_EFFECT_GLOSSY;
    }
    if (strcmp(effectStr, "off") == 0) {
      // "off" can mean either disable effect or use NONE
      return LED_EFFECT_NONE;
    }

    return LED_EFFECT_NONE;  // Unknown effect = solid color
  }
};

#endif // LED_EFFECT_PARSER_H
