#include "../behavior_engine.h"
#include "../behavior_objects.h"
#include "../../face_engine.h"
#include <cstdlib>

namespace {
uint32_t s_dirtTimer = 0;
uint32_t s_exprTimer = 0;
}

static void onEnter() {
  FaceEngine::setAutoMode(true);
  FaceEngine::setExpression(FaceExpression::Disgusted);
  BehaviorEngine::getStats().mouthState = -0.2f;
  s_dirtTimer = 0;
  s_exprTimer = 0;
}

static void onUpdate(uint32_t dtMs) {
  auto& stats = BehaviorEngine::getStats();
  stats.addHealth(-0.2f, dtMs);

  // Expressions
  s_exprTimer += dtMs;
  if (s_exprTimer > 3500) {
    s_exprTimer = 0;
    FaceExpression exprs[] = { FaceExpression::Disgusted, FaceExpression::Annoyed, FaceExpression::Rejected };
    FaceEngine::setExpression(exprs[rand() % 3]);
  }

  // Particules marrons
  s_dirtTimer += dtMs;
  if (s_dirtTimer > 1500) {
    s_dirtTimer = 0;
    float dx = 150.0f + rand() % 166;
    float dy = 280.0f + rand() % 80;
    BehaviorObjects::spawn(ObjectShape::Circle, 0x8B6914, 6 + rand() % 5,
      dx, dy, ((rand() % 30) - 15) / 1000.0f, -0.01f,
      0.0003f, 0, false, 2500);
  }
}

static void onExit() {
  BehaviorObjects::destroyAll();
  FaceEngine::setAutoMode(false);
}

const Behavior BEHAVIOR_DIRTY = {
  "dirty", onEnter, onUpdate, onExit,
  FaceExpression::Disgusted,
  5.0f, 15.0f
};
