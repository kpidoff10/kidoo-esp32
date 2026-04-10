#include "../behavior_engine.h"
#include "../behavior_objects.h"
#include "../../face_engine.h"
#include "../../gotchi_haptic.h"
#include <cstdlib>
#include <cmath>

namespace {
int s_ballId = -1;
int s_bounceCount = 0;
int s_cycleCount = 0;
uint32_t s_phaseTimer = 0;
bool s_throwing = true;

// Callback physique : appelé à chaque collision balle/sol par BehaviorObjects
void onBallBounce(int /*objId*/) {
  GotchiHaptic::ballBounce();
  s_bounceCount++;
  FaceEngine::blink();
}

void throwBall() {
  bool fromLeft = (rand() % 2) == 0;
  float startX = fromLeft ? 60.0f : 406.0f;
  float vx = fromLeft ? 0.1f + (rand() % 50) / 1000.0f : -(0.1f + (rand() % 50) / 1000.0f);

  if (s_ballId >= 0) BehaviorObjects::destroy(s_ballId);

  s_ballId = BehaviorObjects::spawn(
    ObjectShape::Circle, 0xFF3030, 24,
    startX, 150.0f, vx, -0.22f,
    0.0014f, 0.72f, true, 0
  );
  s_bounceCount = 0;
  s_phaseTimer = 0;
  s_throwing = true;

  FaceEngine::setExpression(FaceExpression::Excited);
  GotchiHaptic::ballThrow();
}
}

static void onEnter() {
  s_cycleCount = 0;
  FaceEngine::setAutoMode(true);
  BehaviorObjects::setBounceCallback(onBallBounce);
  throwBall();
}

static void onUpdate(uint32_t dtMs) {
  auto& stats = BehaviorEngine::getStats();
  s_phaseTimer += dtMs;

  // Stats pendant le jeu
  stats.addBoredom(-5.0f, dtMs);
  stats.addHappiness(2.0f, dtMs);
  stats.addEnergy(-1.0f, dtMs);
  stats.addExcitement(1.0f, dtMs);

  // Si la balle est tenue par le doigt → juste suivre des yeux, pas de logique rebond
  if (s_ballId >= 0 && BehaviorObjects::isHeld(s_ballId)) {
    s_phaseTimer = 0;
    s_bounceCount = 0;
    return;
  }

  // Expressions selon la trajectoire (rebonds eux-mêmes gérés par onBallBounce callback)
  float lx, ly;
  if (BehaviorObjects::getLookTarget(lx, ly)) {
    if (ly < -0.3f && s_throwing) {
      FaceEngine::setExpression(FaceExpression::Amazed);
      s_throwing = false;
    }
    if (ly > 0.6f && !s_throwing) {
      FaceEngine::setExpression(FaceExpression::Excited);
      s_throwing = true;
    }
  }

  // Trop fatigue → arrete de jouer
  if (stats.energy < 20) {
    FaceEngine::setExpression(FaceExpression::Tired);
    BehaviorObjects::destroy(s_ballId);
    s_ballId = -1;
    BehaviorEngine::requestBehavior(&BEHAVIOR_IDLE);
    return;
  }

  // Après 4 rebonds ou timeout → balle s'arrete, attendre action user
  if (s_bounceCount >= 4 || s_phaseTimer > 5000) {
    if (s_ballId >= 0) {
      BehaviorObjects::destroy(s_ballId);
      s_ballId = -1;
    }
    // Le gotchi attend que le user relance (regard idle)
    FaceEngine::setExpression(FaceExpression::Normal);
    FaceEngine::lookAt(0, 0.2f);  // Regarde en bas (ou etait la balle)
  }
}

static void onExit() {
  BehaviorObjects::setBounceCallback(nullptr);
  if (s_ballId >= 0) BehaviorObjects::destroy(s_ballId);
  s_ballId = -1;
  FaceEngine::setAutoMode(false);
}

// Expose l'ID de la balle pour le drag
int playBallGetId() { return s_ballId; }

// Lancer la balle depuis une position (appelable depuis behavior_engine via onSwipe)
void playBallLaunchFrom(float fromX, float dirX) {
  if (s_ballId >= 0) BehaviorObjects::destroy(s_ballId);

  float vx = dirX * (0.08f + (rand() % 40) / 1000.0f);
  s_ballId = BehaviorObjects::spawn(
    ObjectShape::Circle, 0xFF3030, 24,
    fromX, 160.0f, vx, -0.20f,
    0.0014f, 0.72f, true, 0
  );
  s_bounceCount = 0;
  s_phaseTimer = 0;
  s_throwing = true;
  FaceEngine::setExpression(FaceExpression::Excited);
  GotchiHaptic::ballThrow();
}

static bool playOnTouch() {
  auto& stats = BehaviorEngine::getStats();
  // Relancer la balle quand on tape
  bool fromLeft = (rand() % 2) == 0;
  playBallLaunchFrom(fromLeft ? 60.0f : 406.0f, fromLeft ? 1.0f : -1.0f);
  stats.happiness += 5;
  stats.excitement += 10;
  stats.clamp();
  return true;
}

static bool playOnShake() {
  auto& stats = BehaviorEngine::getStats();
  FaceEngine::setExpression(FaceExpression::Amazed);
  stats.excitement += 20;
  stats.happiness += 5;
  stats.energy -= 3;
  stats.clamp();
  if (stats.excitement > 85) {
    BehaviorEngine::requestBehavior(&BEHAVIOR_TANTRUM);
  }
  return true;
}

const Behavior BEHAVIOR_PLAY_BALL = {
  "play", onEnter, onUpdate, onExit, playOnTouch, playOnShake,
  FaceExpression::Excited,
  8.0f,   // min 8s
  60.0f,  // max 60s (safety valve)
  BF_USER_ACTION
};
