#include "../behavior_engine.h"
#include "../behavior_objects.h"
#include "../../face_engine.h"
#include <cstdlib>

namespace {
uint32_t s_lookTimer = 0;
uint32_t s_sparkTimer = 0;
uint32_t s_phase = 0;
}

static void onEnter() {
  FaceEngine::setAutoMode(true);
  FaceEngine::setExpression(FaceExpression::Confused);
  BehaviorEngine::getStats().mouthState = 0.0f;
  s_lookTimer = 0;
  s_sparkTimer = 0;
  s_phase = 0;
}

static void onUpdate(uint32_t dtMs) {
  auto& stats = BehaviorEngine::getStats();
  s_phase += dtMs;

  stats.addBoredom(-3.0f, dtMs);
  stats.addHappiness(1.0f, dtMs);

  // Regard vif, regarde partout rapidement
  s_lookTimer += dtMs;
  if (s_lookTimer > 800) {
    s_lookTimer = 0;
    float lx = ((float)(rand() % 300) - 150) / 150.0f;
    float ly = ((float)(rand() % 200) - 100) / 200.0f;
    FaceEngine::lookAt(lx, ly);
  }

  // Séquence d'expressions de découverte
  if (s_phase < 2000) {
    // Confused au début
  } else if (s_phase < 2100) {
    FaceEngine::setExpression(FaceExpression::Skeptical);
    stats.mouthState = 0.1f;
  } else if (s_phase < 5000) {
    // Observe
  } else if (s_phase < 5100) {
    FaceEngine::setExpression(FaceExpression::Amazed);
    stats.mouthState = -0.4f; // Bouche ouverte d'émerveillement
    FaceEngine::blink();
  } else if (s_phase < 7000) {
    stats.mouthState = 0.3f;
  }

  // Étoiles de curiosité (jaunes brillantes)
  s_sparkTimer += dtMs;
  if (s_sparkTimer > 1200) {
    s_sparkTimer = 0;
    float sx = 150.0f + rand() % 166;
    float sy = 130.0f + rand() % 60;
    BehaviorObjects::spawn(ObjectShape::Circle, 0xFFDD44, 8 + rand() % 6,
      sx, sy, ((rand() % 30) - 15) / 1000.0f, -0.03f,
      0, 0, false, 2000);
  }
}

static void onExit() {
  BehaviorObjects::destroyAll();
  FaceEngine::setAutoMode(false);
}

static bool curiousOnTouch() {
  auto& stats = BehaviorEngine::getStats();
  FaceEngine::setExpression(FaceExpression::Amazed);
  stats.happiness += 8;
  stats.excitement += 15;
  stats.clamp();
  return true;
}

static bool curiousOnShake() {
  auto& stats = BehaviorEngine::getStats();
  FaceEngine::setExpression(FaceExpression::Excited);
  stats.excitement += 20;
  stats.boredom -= 15;
  stats.clamp();
  return true;
}

const Behavior BEHAVIOR_CURIOUS = {
  "curious", onEnter, onUpdate, onExit, curiousOnTouch, curiousOnShake,
  FaceExpression::Confused,
  4.0f, 10.0f
};
