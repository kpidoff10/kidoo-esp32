#include "../behavior_engine.h"
#include "../behavior_objects.h"
#include "../../face_engine.h"
#include <cstdlib>

namespace {
uint32_t s_angerTimer = 0;
uint32_t s_exprTimer = 0;
uint32_t s_shakeTimer = 0;
}

static void onEnter() {
  FaceEngine::setAutoMode(true);
  FaceEngine::setExpression(FaceExpression::Angry);
  BehaviorEngine::getStats().mouthState = -0.5f;
  s_angerTimer = 0;
  s_exprTimer = 0;
  s_shakeTimer = 0;
}

static void onUpdate(uint32_t dtMs) {
  auto& stats = BehaviorEngine::getStats();

  stats.addExcitement(-5.0f, dtMs);
  stats.addHappiness(0.5f, dtMs); // Se défoule

  // Regard agité (secoue la tête)
  s_shakeTimer += dtMs;
  if (s_shakeTimer > 200) {
    s_shakeTimer = 0;
    float lx = ((float)(rand() % 100) - 50) / 100.0f;
    float ly = ((float)(rand() % 60) - 30) / 200.0f;
    FaceEngine::lookAt(lx, ly);
  }

  // Expressions de colère
  s_exprTimer += dtMs;
  if (s_exprTimer > 1500) {
    s_exprTimer = 0;
    FaceExpression exprs[] = { FaceExpression::Angry, FaceExpression::Furious, FaceExpression::Annoyed };
    FaceEngine::setExpression(exprs[rand() % 3]);
    stats.mouthState = -0.4f - 0.3f * ((float)(rand() % 100) / 100.0f);
  }

  // Particules rouges de colère
  s_angerTimer += dtMs;
  if (s_angerTimer > 600) {
    s_angerTimer = 0;
    float ax = 160.0f + rand() % 146;
    float ay = 140.0f + rand() % 50;
    BehaviorObjects::spawn(ObjectShape::Circle, 0xFF4040, 10 + rand() % 8,
      ax, ay, ((rand() % 60) - 30) / 1000.0f, -0.06f,
      0, 0, false, 1500);
  }
}

static void onExit() {
  BehaviorObjects::destroyAll();
  FaceEngine::setAutoMode(false);
}

const Behavior BEHAVIOR_TANTRUM = {
  "tantrum", onEnter, onUpdate, onExit,
  FaceExpression::Angry,
  3.0f, 8.0f
};
