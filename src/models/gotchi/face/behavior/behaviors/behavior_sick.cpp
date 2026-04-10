#include "../behavior_engine.h"
#include "../behavior_objects.h"
#include "../../face_engine.h"
#include "../../gotchi_haptic.h"
#include <cstdlib>

namespace {
uint32_t s_germTimer = 0;
uint32_t s_exprTimer = 0;
uint32_t s_phase = 0;
}

static void onEnter() {
  FaceEngine::setAutoMode(false);
  FaceEngine::setExpression(FaceExpression::Vulnerable);
  FaceEngine::lookAt(0, 0.2f);
  BehaviorEngine::getStats().mouthState = -0.3f;
  s_germTimer = 0;
  s_exprTimer = 0;
  s_phase = 0;
}

static void onUpdate(uint32_t dtMs) {
  auto& stats = BehaviorEngine::getStats();
  s_phase += dtMs;

  stats.addEnergy(-1.0f, dtMs);
  stats.addHappiness(-1.0f, dtMs);

  // Auto-guérison très lente
  stats.addHealth(0.1f, dtMs);

  // Bouche grimaçante
  stats.mouthState = -0.3f + 0.1f * ((float)(rand() % 100) / 100.0f);

  // Phases d'expression
  s_exprTimer += dtMs;
  if (s_exprTimer > 4000) {
    s_exprTimer = 0;
    FaceExpression exprs[] = { FaceExpression::Vulnerable, FaceExpression::Guilty, FaceExpression::Tired };
    FaceEngine::setExpression(exprs[rand() % 3]);
    // Regard faible, lent
    FaceEngine::lookAt(((float)(rand() % 60) - 30) / 200.0f, 0.15f);
    // Toux haptique faible et lente, 1 fois sur 2
    if ((rand() % 2) == 0) GotchiHaptic::cough();
  }

  // Germes verts qui flottent
  s_germTimer += dtMs;
  if (s_germTimer > 1800) {
    s_germTimer = 0;
    float angle = (float)(rand() % 360) * 3.14159f / 180.0f;
    float dist = 130.0f + rand() % 40;
    float gx = 233.0f + dist * cosf(angle);
    float gy = 233.0f + dist * sinf(angle);
    BehaviorObjects::spawn(ObjectShape::Circle, 0x60FF60, 8 + rand() % 6,
      gx, gy, ((rand() % 20) - 10) / 1000.0f, ((rand() % 20) - 10) / 1000.0f,
      0, 0, false, 3000);
  }

  // Clignements lents
  if (s_phase % 5000 < 50) FaceEngine::blink();
}

static void onExit() {
  BehaviorObjects::destroyAll();
}

static bool sickOnTouch() {
  auto& stats = BehaviorEngine::getStats();
  if (stats.touchCount <= 3) {
    FaceEngine::setExpression(FaceExpression::Vulnerable);
    FaceEngine::nod(FaceEngine::GestureSpeed::Slow);
    stats.happiness += 5;
    stats.health += 2;
    stats.clamp();
  } else {
    // Too weak for more
    FaceEngine::setExpression(FaceExpression::Tired);
    stats.happiness += 1;
    stats.clamp();
  }
  return true;
}

static bool sickOnShake() {
  auto& stats = BehaviorEngine::getStats();
  FaceEngine::setExpression(FaceExpression::Horrified);
  stats.health -= 10;
  stats.happiness -= 15;
  stats.irritability += 25;
  stats.mouthState = -0.6f;
  stats.clamp();
  return true;
}

const Behavior BEHAVIOR_SICK = {
  "sick", onEnter, onUpdate, onExit, sickOnTouch, sickOnShake,
  FaceExpression::Vulnerable,
  10.0f, 40.0f,
  BF_URGENT
};
