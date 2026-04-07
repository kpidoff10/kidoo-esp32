#include "../behavior_engine.h"
#include "../../face_engine.h"
#include <cstdlib>

namespace {
uint32_t s_lookTimer = 0;
uint32_t s_exprTimer = 0;
uint32_t s_phase = 0;
}

static void onEnter() {
  FaceEngine::setAutoMode(true);
  FaceEngine::setExpression(FaceExpression::Pleading);
  BehaviorEngine::getStats().mouthState = -0.15f;
  s_lookTimer = 0;
  s_exprTimer = 0;
  s_phase = 0;
}

static void onUpdate(uint32_t dtMs) {
  auto& stats = BehaviorEngine::getStats();
  s_phase += dtMs;

  stats.addBoredom(-1.0f, dtMs);
  stats.addHappiness(-0.3f, dtMs);

  // Regard lent, cherche quelqu'un
  s_lookTimer += dtMs;
  if (s_lookTimer > 2500) {
    s_lookTimer = 0;
    float lx = ((float)(rand() % 200) - 100) / 150.0f;
    float ly = ((float)(rand() % 100) - 50) / 200.0f;
    FaceEngine::lookAt(lx, ly);
  }

  // Expressions de solitude
  s_exprTimer += dtMs;
  if (s_exprTimer > 5000) {
    s_exprTimer = 0;
    FaceExpression exprs[] = { FaceExpression::Pleading, FaceExpression::Embarrassed, FaceExpression::Sad, FaceExpression::Disappointed };
    FaceEngine::setExpression(exprs[rand() % 4]);
    stats.mouthState = -0.1f - 0.15f * ((float)(rand() % 100) / 100.0f);
  }
}

static void onExit() {
  FaceEngine::setAutoMode(false);
}

const Behavior BEHAVIOR_LONELY = {
  "lonely", onEnter, onUpdate, onExit,
  FaceExpression::Pleading,
  8.0f, 20.0f
};
