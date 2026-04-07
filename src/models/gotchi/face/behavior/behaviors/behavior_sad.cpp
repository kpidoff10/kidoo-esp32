#include "../behavior_engine.h"
#include "../behavior_objects.h"
#include "../../face_engine.h"
#include <cstdlib>

namespace {
uint32_t s_tearTimer = 0;
}

static void onEnter() {
  FaceEngine::setAutoMode(true);
  FaceEngine::setExpression(FaceExpression::Sad);
  FaceEngine::lookAt(0, 0.3f); // Regard vers le bas
  s_tearTimer = 0;
}

static void onUpdate(uint32_t dtMs) {
  auto& stats = BehaviorEngine::getStats();

  // Se console doucement
  stats.addHappiness(0.5f, dtMs);

  // Larmes : gouttes bleues qui tombent sous les yeux
  s_tearTimer += dtMs;
  if (s_tearTimer > 1200) {
    s_tearTimer = 0;

    // Larme sous l'oeil gauche
    float tearX = 138.0f + (rand() % 20) - 10;
    BehaviorObjects::spawn(
      ObjectShape::Drop, 0x60C0FF, 10,
      tearX, 290.0f, 0, 0.05f,
      0.0008f, 0, false, 2000
    );

    // Larme sous l'oeil droit (avec délai aléatoire)
    if (rand() % 3 != 0) {
      float tearX2 = 328.0f + (rand() % 20) - 10;
      BehaviorObjects::spawn(
        ObjectShape::Drop, 0x60C0FF, 10,
        tearX2, 290.0f, 0, 0.04f,
        0.0008f, 0, false, 2000
      );
    }
  }

  // Expressions variées de tristesse
  if (rand() % 500 == 0) {
    FaceExpression sadExprs[] = { FaceExpression::Sad, FaceExpression::Despair, FaceExpression::Disappointed };
    FaceEngine::setExpression(sadExprs[rand() % 3]);
  }
}

static void onExit() {
  BehaviorObjects::destroyAll();
  FaceEngine::setAutoMode(false);
}

const Behavior BEHAVIOR_SAD = {
  "sad", onEnter, onUpdate, onExit,
  FaceExpression::Sad,
  5.0f,   // min 5s
  15.0f   // max 15s
};
