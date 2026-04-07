#include "../behavior_engine.h"
#include "../behavior_objects.h"
#include "../../face_engine.h"
#include <cstdlib>

namespace {
uint32_t s_timer = 0;
uint32_t s_chewTimer = 0;
bool s_chewing = false;
}

static void onEnter() {
  FaceEngine::setAutoMode(true);
  FaceEngine::setExpression(FaceExpression::Happy);
  FaceEngine::lookAt(0, 0.1f);
  BehaviorEngine::getStats().mouthState = 0.6f;
  s_timer = 0;
  s_chewTimer = 0;
  s_chewing = false;

  // Spawn particules de nourriture qui montent
  for (int i = 0; i < 3; i++) {
    float x = 200.0f + rand() % 66;
    uint32_t colors[] = { 0x80FF80, 0xFF80C0, 0xFFFF60, 0xFFAA40 };
    BehaviorObjects::spawn(ObjectShape::Circle, colors[rand() % 4], 10 + rand() % 6,
      x, 360.0f - i * 30, ((rand() % 20) - 10) / 1000.0f, -0.05f - (rand() % 20) / 1000.0f,
      0, 0, false, 3000);
  }
}

static void onUpdate(uint32_t dtMs) {
  auto& stats = BehaviorEngine::getStats();
  s_timer += dtMs;

  // Animation de mastication : bouche ouvre/ferme
  s_chewTimer += dtMs;
  if (s_chewTimer > 300) {
    s_chewTimer = 0;
    s_chewing = !s_chewing;
    stats.mouthState = s_chewing ? -0.3f : 0.4f;
  }

  // Expressions de plaisir
  if (s_timer > 1500 && s_timer < 1600) {
    FaceEngine::setExpression(FaceExpression::Excited);
  }
  if (s_timer > 3000 && s_timer < 3100) {
    FaceEngine::setExpression(FaceExpression::Happy);
    FaceEngine::blink();
  }
}

static void onExit() {
  BehaviorObjects::destroyAll();
  BehaviorEngine::getStats().mouthState = 0.5f; // Sourire satisfait
  FaceEngine::setAutoMode(false);
}

const Behavior BEHAVIOR_EATING = {
  "eating", onEnter, onUpdate, onExit,
  FaceExpression::Happy,
  3.0f, 5.0f
};
