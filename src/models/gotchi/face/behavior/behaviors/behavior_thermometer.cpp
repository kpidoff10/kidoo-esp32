#include "../behavior_engine.h"
#include "../behavior_objects.h"
#include "../top_chip.h"
#include "../sprites/sprite_thermometer_66.h"
#include "../../face_engine.h"
#include <cstdlib>
#include <cstdio>

namespace {

int s_thermoObjId = -1;
uint32_t s_timer = 0;
uint32_t s_exitTimer = 0;
bool s_hasReading = false;
uint8_t s_phase = 0;  // 0=approche, 1=glisse, 2=en place, 3=retrait
bool s_removing = false;  // Flag pour déclencher l'animation de retrait

constexpr float MOUTH_X = 233.0f;
constexpr float MOUTH_Y = 318.0f;

// Health (0-100) → Température (41.0 - 36.0°C)
float healthToTemp(float health) {
  return 41.0f - (health / 100.0f) * 5.0f;
}

// RGB565 couleur selon la température
uint16_t tempToColor(float temp) {
  if (temp < 37.5f) return 0x07E0;  // Vert
  if (temp < 38.5f) return 0xFD20;  // Orange
  return 0xF800;                     // Rouge
}

} // namespace

static void onEnter() {
  FaceEngine::setAutoMode(true);
  FaceEngine::setExpression(FaceExpression::Vulnerable);
  FaceEngine::lookAt(0, 0.3f);
  BehaviorEngine::getStats().mouthState = -0.2f;
  s_timer = 0;
  s_exitTimer = 0;
  s_hasReading = false;
  s_phase = 0;
  s_removing = false;
  s_thermoObjId = -1;

  // Phase 0 : thermomètre apparaît à droite, statique (le gotchi le regarde)
  s_thermoObjId = BehaviorObjects::spawnSprite(
    SPRITE_THERMOMETER_66_ASSET, 0,
    MOUTH_X + 90.0f, MOUTH_Y, 0, 0, 0, 0, true, 10000);
}

static void onUpdate(uint32_t dtMs) {
  BehaviorEngine::resetTimer();
  s_timer += dtMs;

  // Phase 0 → 1 : après 800ms, le thermomètre glisse vers la bouche
  if (s_phase == 0 && s_timer >= 800) {
    s_phase = 1;
    if (s_thermoObjId >= 0) BehaviorObjects::destroy(s_thermoObjId);
    s_thermoObjId = BehaviorObjects::spawnSprite(
      SPRITE_THERMOMETER_66_ASSET, 0,
      MOUTH_X + 90.0f, MOUTH_Y, -0.07f, 0.02f, 0, 0, true, 5000);
    FaceEngine::setExpression(FaceExpression::Vulnerable);
    BehaviorEngine::getStats().mouthState = -0.3f;  // Ouvre la bouche
  }

  // Phase 1 → 2 : après 2s, le thermomètre est en place, on le fixe
  if (s_phase == 1 && s_timer >= 2000) {
    s_phase = 2;
    if (s_thermoObjId >= 0) BehaviorObjects::destroy(s_thermoObjId);
    // Fixer le thermomètre à la bouche
    s_thermoObjId = BehaviorObjects::spawnSprite(
      SPRITE_THERMOMETER_66_ASSET, 0,
      MOUTH_X, MOUTH_Y + 30.0f, 0, 0, 0, 0, false, 0);
    BehaviorEngine::getStats().mouthState = -0.2f;

    // Chip "..." pendant la mesure
    TopChip::show("...", 0x8410);
  }

  // Phase 2 : après 4s total, afficher la température
  if (s_phase == 2 && !s_hasReading && s_timer >= 4000) {
    s_hasReading = true;
    float health = BehaviorEngine::getStats().health;
    float temp = healthToTemp(health);
    uint16_t color = tempToColor(temp);

    char text[16];
    snprintf(text, sizeof(text), "%.1f^C", (double)temp);
    TopChip::show(text, color);  // bgColor=0 → auto darken

    if (temp >= 38.5f) {
      FaceEngine::setExpression(FaceExpression::Vulnerable);
    } else if (temp >= 37.5f) {
      FaceEngine::setExpression(FaceExpression::Confused);
    } else {
      FaceEngine::setExpression(FaceExpression::Happy);
    }
  }

  // Demande de retrait → lancer phase 3
  if (s_removing && s_phase != 3) {
    s_phase = 3;
    s_exitTimer = 0;
    TopChip::hide();
    // Détruire le sprite fixe et le relancer avec vélocité vers la droite
    if (s_thermoObjId >= 0) BehaviorObjects::destroy(s_thermoObjId);
    s_thermoObjId = BehaviorObjects::spawnSprite(
      SPRITE_THERMOMETER_66_ASSET, 0,
      MOUTH_X, MOUTH_Y + 30.0f, 0.08f, -0.02f, 0, 0, false, 2000);
    BehaviorEngine::getStats().mouthState = 0.5f;  // Ferme la bouche
    FaceEngine::setExpression(FaceExpression::Happy);
  }

  // Phase 3 : retrait — le thermomètre glisse vers la droite puis on quitte
  if (s_phase == 3) {
    s_exitTimer += dtMs;
    if (s_exitTimer >= 1200) {
      // Animation terminée → demander la sortie propre
      BehaviorEngine::stopThermometer();
    }
    return;
  }

  // Blink périodique
  if (s_timer % 4000 < dtMs) {
    FaceEngine::blink();
  }
}

static void onExit() {
  BehaviorObjects::destroyAll();
  TopChip::hide();
  BehaviorEngine::getStats().mouthState = 0.5f;
  FaceEngine::setAutoMode(false);
}

static bool thermoOnTouch() {
  FaceEngine::setExpression(FaceExpression::Suspicious);
  return true;
}

static bool thermoOnShake() {
  auto& stats = BehaviorEngine::getStats();
  FaceEngine::setExpression(FaceExpression::Horrified);
  stats.happiness -= 5;
  stats.clamp();
  return true;
}

// Appelé par BehaviorEngine::stopThermometer()
void thermometerRequestRemove() {
  s_removing = true;
}

bool thermometerIsRemoving() {
  return s_removing;
}

const Behavior BEHAVIOR_THERMOMETER = {
  "thermometer", onEnter, onUpdate, onExit, thermoOnTouch, thermoOnShake,
  FaceExpression::Vulnerable,
  3.0f, 0.0f,
  BF_USER_ACTION
};
