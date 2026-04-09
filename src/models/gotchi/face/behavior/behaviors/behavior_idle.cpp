#include "../behavior_engine.h"
#include "../../face_engine.h"
#include <cstdlib>

namespace {
uint32_t s_nextLook = 0;
uint32_t s_nextMicroExpr = 0;
uint32_t s_nextYawn = 0;
uint32_t s_idlePhase = 0;
bool s_justYawned = false;
}

static void onEnter() {
  FaceEngine::setExpression(FaceExpression::Normal);
  FaceEngine::setAutoMode(true);
  auto& stats = BehaviorEngine::getStats();
  stats.mouthState = 0.0f;
  s_nextLook = 1500 + rand() % 3000;
  s_nextMicroExpr = 6000 + rand() % 10000;
  s_nextYawn = 15000 + rand() % 20000;
  s_idlePhase = 0;
  s_justYawned = false;
}

static void onUpdate(uint32_t dtMs) {
  auto& stats = BehaviorEngine::getStats();
  s_idlePhase += dtMs;

  // Ennui monte doucement
  stats.addBoredom(0.5f, dtMs);

  // Bouche réactive aux stats
  if (stats.happiness > 60) stats.mouthState += (0.2f - stats.mouthState) * 0.01f;
  else stats.mouthState += (0.0f - stats.mouthState) * 0.01f;

  // --- Micro-mouvements du regard ---
  if (s_nextLook <= dtMs) {
    float lx = ((float)(rand() % 300) - 150) / 500.0f;
    float ly = ((float)(rand() % 200) - 100) / 600.0f;
    FaceEngine::lookAt(lx, ly);
    s_nextLook = 2000 + rand() % 4000;
  } else {
    s_nextLook -= dtMs;
  }

  // --- Micro-expressions aléatoires ---
  if (s_nextMicroExpr <= dtMs) {
    int r = rand() % 8;
    switch (r) {
      case 0: FaceEngine::setExpression(FaceExpression::Confused); break;
      case 1: FaceEngine::setExpression(FaceExpression::Skeptical); break;
      case 2: FaceEngine::setExpression(FaceExpression::Bored); stats.mouthState = -0.1f; break;
      case 3: FaceEngine::setExpression(FaceExpression::Amazed); stats.mouthState = -0.3f; break;
      case 4: FaceEngine::setExpression(FaceExpression::Suspicious); break;
      case 5: FaceEngine::setExpression(FaceExpression::Happy); stats.mouthState = 0.4f; break;
      default: FaceEngine::setExpression(FaceExpression::Normal); stats.mouthState = 0.0f; break;
    }
    s_nextMicroExpr = 8000 + rand() % 15000;
  } else {
    s_nextMicroExpr -= dtMs;
  }

  // --- Bâillement si fatigué ---
  if (s_nextYawn <= dtMs) {
    if (stats.energy < 50 && !s_justYawned) {
      FaceEngine::setExpression(FaceExpression::Tired);
      stats.mouthState = -0.8f; // Grande bouche ouverte
      s_justYawned = true;
      s_nextYawn = 3000; // Durée du bâillement
    } else if (s_justYawned) {
      FaceEngine::setExpression(FaceExpression::Normal);
      stats.mouthState = 0.0f;
      FaceEngine::blink();
      s_justYawned = false;
      s_nextYawn = 20000 + rand() % 30000;
    } else {
      s_nextYawn = 15000 + rand() % 20000;
    }
  } else {
    s_nextYawn -= dtMs;
  }
}

static void onExit() {
  FaceEngine::setAutoMode(false);
}

static bool idleOnTouch() {
  auto& stats = BehaviorEngine::getStats();
  if (stats.happiness < 40) {
    FaceEngine::setExpression(FaceExpression::Pleading);
    FaceEngine::nod(FaceEngine::GestureSpeed::Slow);
    stats.happiness += 5;
    stats.clamp();
  } else {
    FaceEngine::nod(FaceEngine::GestureSpeed::Normal);
    stats.happiness += 8;
    stats.excitement += 15;
    stats.clamp();
    BehaviorEngine::requestBehavior(&BEHAVIOR_HAPPY);
  }
  return true;
}

static bool idleOnShake() {
  auto& stats = BehaviorEngine::getStats();
  // Secoue = surpris, mais ne lance PAS le jeu automatiquement
  FaceEngine::setExpression(FaceExpression::Surprised);
  stats.excitement += 15;
  stats.boredom -= 5;
  stats.clamp();
  return true;
}

const Behavior BEHAVIOR_IDLE = {
  "idle", onEnter, onUpdate, onExit, idleOnTouch, idleOnShake,
  FaceExpression::Normal,
  5.0f,   // min 5s
  30.0f,  // max 30s
  BF_NONE
};
