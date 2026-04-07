#include "../behavior_engine.h"
#include "../behavior_objects.h"
#include "../../face_engine.h"
#include <cstdlib>

namespace {
uint32_t s_phase = 0;
uint32_t s_zzzTimer = 0;
}

static void onEnter() {
  FaceEngine::setAutoMode(false);
  FaceEngine::setExpression(FaceExpression::Tired);
  s_phase = 0;
  s_zzzTimer = 0;
}

static void onUpdate(uint32_t dtMs) {
  auto& stats = BehaviorEngine::getStats();
  s_phase += dtMs;

  // Récupération d'énergie
  stats.addEnergy(3.0f, dtMs);

  // Phase 1 (0-2s) : Tired → yeux se ferment
  if (s_phase < 2000) {
    // Tired déjà set dans onEnter
  }
  // Phase 2 (2-4s) : Asleep
  else if (s_phase < 4000 && s_phase - dtMs < 2000) {
    FaceEngine::setExpression(FaceExpression::Asleep);
  }
  // Phase 3+ : Zzz flottent
  else {
    s_zzzTimer += dtMs;
    if (s_zzzTimer > 2500) {
      s_zzzTimer = 0;
      // Petit "Z" = cercle blanc qui monte et disparaît
      float baseX = 300.0f + (rand() % 40) - 20;
      BehaviorObjects::spawn(
        ObjectShape::Circle, 0xFFFFFF, 8 + rand() % 8,
        baseX, 180.0f,
        0.01f + (rand() % 20) / 1000.0f, -0.04f,
        0, 0, false, 3000
      );
    }
    FaceEngine::lookAt(0, 0.1f); // Regard vers le bas (yeux fermés)
  }
}

static void onExit() {
  // Réveil : transition douce
  FaceEngine::setExpression(FaceExpression::Tired);
  BehaviorObjects::destroyAll();
}

const Behavior BEHAVIOR_SLEEP = {
  "sleep", onEnter, onUpdate, onExit,
  FaceExpression::Asleep,
  10.0f,  // min 10s
  30.0f   // max 30s
};
