#include "../behavior_engine.h"
#include "../behavior_objects.h"
#include "../../face_engine.h"
#include <cstdlib>

namespace {
uint32_t s_heartTimer = 0;
uint32_t s_exprTimer = 0;
}

static void onEnter() {
  FaceEngine::setAutoMode(true);
  FaceEngine::setExpression(FaceExpression::Happy);
  s_heartTimer = 0;
  s_exprTimer = 0;
}

static void onUpdate(uint32_t dtMs) {
  auto& stats = BehaviorEngine::getStats();

  stats.addExcitement(-2.0f, dtMs);

  // Coeurs qui montent
  s_heartTimer += dtMs;
  if (s_heartTimer > 800) {
    s_heartTimer = 0;

    float hx = 180.0f + (rand() % 100);
    float hy = 160.0f + (rand() % 40);
    BehaviorObjects::spawn(
      ObjectShape::Circle, 0xFF6090, 12 + rand() % 8,
      hx, hy,
      ((rand() % 60) - 30) / 1000.0f, -0.06f - (rand() % 30) / 1000.0f,
      0, 0, false, 2500
    );
  }

  // Alternance happy/excited/amazed
  s_exprTimer += dtMs;
  if (s_exprTimer > 2000) {
    s_exprTimer = 0;
    FaceExpression happyExprs[] = { FaceExpression::Happy, FaceExpression::Excited, FaceExpression::Amazed };
    FaceEngine::setExpression(happyExprs[rand() % 3]);
  }
}

static void onExit() {
  BehaviorObjects::destroyAll();
  FaceEngine::setAutoMode(false);
}

const Behavior BEHAVIOR_HAPPY = {
  "happy", onEnter, onUpdate, onExit,
  FaceExpression::Happy,
  3.0f,  // min 3s
  8.0f   // max 8s
};
