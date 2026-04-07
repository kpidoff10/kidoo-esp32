#include "../behavior_engine.h"
#include "../behavior_objects.h"
#include "../../face_engine.h"
#include <cstdlib>

namespace {
uint32_t s_crumbTimer = 0;
uint32_t s_exprTimer = 0;
}

static void onEnter() {
  FaceEngine::setAutoMode(true);
  FaceEngine::setExpression(FaceExpression::Pleading);
  FaceEngine::lookAt(0, 0.4f);
  BehaviorEngine::getStats().mouthState = -0.6f;
  s_crumbTimer = 0;
  s_exprTimer = 0;
}

static void onUpdate(uint32_t dtMs) {
  auto& stats = BehaviorEngine::getStats();
  stats.addHappiness(-0.5f, dtMs);

  // Bouche ouverte (faim)
  stats.mouthState = -0.5f - 0.3f * (float)(rand() % 100) / 100.0f;

  // Regard qui cherche de la nourriture
  s_exprTimer += dtMs;
  if (s_exprTimer > 3000) {
    s_exprTimer = 0;
    float lx = ((float)(rand() % 200) - 100) / 200.0f;
    FaceEngine::lookAt(lx, 0.2f + ((float)(rand() % 40)) / 100.0f);
    FaceExpression exprs[] = { FaceExpression::Pleading, FaceExpression::Sad, FaceExpression::Guilty };
    FaceEngine::setExpression(exprs[rand() % 3]);
  }

  // Petites miettes jaunes
  s_crumbTimer += dtMs;
  if (s_crumbTimer > 2000) {
    s_crumbTimer = 0;
    float cx = 180.0f + rand() % 100;
    BehaviorObjects::spawn(ObjectShape::Circle, 0xFFCC00, 6 + rand() % 4,
      cx, 350.0f, ((rand() % 40) - 20) / 1000.0f, -0.02f,
      0.0005f, 0, false, 2500);
  }
}

static void onExit() {
  BehaviorObjects::destroyAll();
  FaceEngine::setAutoMode(false);
}

const Behavior BEHAVIOR_HUNGRY = {
  "hungry", onEnter, onUpdate, onExit,
  FaceExpression::Pleading,
  5.0f, 20.0f
};
