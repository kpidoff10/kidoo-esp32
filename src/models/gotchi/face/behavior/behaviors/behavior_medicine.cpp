#include "../behavior_engine.h"
#include "../behavior_objects.h"
#include "../top_chip.h"
#include "../sprites/sprite_pill_emoji_44.h"
#include "../sprites/sprite_sparkle_26.h"
#include "../../face_engine.h"
#include "../../gotchi_haptic.h"

namespace {

int s_pillObjId = -1;
uint32_t s_timer = 0;
uint8_t s_phase = 0;  // 0=regarde, 1=approche, 2=avale, 3=effet

constexpr float MOUTH_X = 233.0f;
constexpr float MOUTH_Y = 318.0f;

} // namespace

static void onEnter() {
  FaceEngine::setAutoMode(true);
  FaceEngine::setExpression(FaceExpression::Vulnerable);
  FaceEngine::lookAt(0.3f, 0.1f);
  BehaviorEngine::getStats().mouthState = -0.1f;
  s_timer = 0;
  s_phase = 0;
  s_pillObjId = -1;

  // Phase 0 : capsule apparaît à droite
  s_pillObjId = BehaviorObjects::spawnSprite(
    SPRITE_PILL_EMOJI_44_ASSET, 0,
    MOUTH_X + 90.0f, MOUTH_Y - 20.0f, 0, 0, 0, 0, true, 8000);
}

static void onUpdate(uint32_t dtMs) {
  auto& stats = BehaviorEngine::getStats();
  s_timer += dtMs;

  // Phase 0 → 1 : après 800ms, la capsule glisse vers la bouche
  if (s_phase == 0 && s_timer >= 800) {
    s_phase = 1;
    if (s_pillObjId >= 0) BehaviorObjects::destroy(s_pillObjId);
    s_pillObjId = BehaviorObjects::spawnSprite(
      SPRITE_PILL_EMOJI_44_ASSET, 0,
      MOUTH_X + 90.0f, MOUTH_Y - 20.0f, -0.07f, 0.015f, 0, 0, true, 4000);
    FaceEngine::setExpression(FaceExpression::Confused);  // Pas content
    stats.mouthState = -0.5f;  // Ouvre la bouche
  }

  // Phase 1 → 2 : après 2s, la capsule est avalée
  if (s_phase == 1 && s_timer >= 2000) {
    s_phase = 2;
    if (s_pillObjId >= 0) {
      BehaviorObjects::destroy(s_pillObjId);
      s_pillObjId = -1;
    }
    stats.mouthState = 0.5f;  // Ferme la bouche (avale)
    GotchiHaptic::chew();

    // Grimace : pas bon !
    FaceEngine::setExpression(FaceExpression::Suspicious);
    FaceEngine::blink();

    // Appliquer le soin
    stats.heal();
  }

  // Phase 2 → 3 : après 3s, sparkles + expression contente
  if (s_phase == 2 && s_timer >= 3000) {
    s_phase = 3;

    // Sparkles de guérison
    for (int i = 0; i < 3; i++) {
      float x = 130.0f + i * 100.0f + (rand() % 30);
      BehaviorObjects::spawnSprite(SPRITE_SPARKLE_26_ASSET, 0,
        x, 160.0f, ((rand() % 20) - 10) / 1000.0f, -0.05f,
        0, 0, false, 2000);
    }

    FaceEngine::setExpression(FaceExpression::Happy);
    stats.mouthState = 0.6f;  // Sourire
  }

  // Phase 3 : après 5s total, behavior terminé
  // (pas de resetTimer → le behavior expire naturellement)
}

static void onExit() {
  BehaviorObjects::destroyAll();
  TopChip::hide();
  BehaviorEngine::getStats().mouthState = 0.5f;
  FaceEngine::setAutoMode(false);
}

static bool medicineOnTouch() {
  FaceEngine::setExpression(FaceExpression::Suspicious);
  return true;
}

static bool medicineOnShake() {
  auto& stats = BehaviorEngine::getStats();
  FaceEngine::setExpression(FaceExpression::Angry);
  stats.happiness -= 5;
  stats.clamp();
  return true;
}

const Behavior BEHAVIOR_MEDICINE = {
  "medicine", onEnter, onUpdate, onExit, medicineOnTouch, medicineOnShake,
  FaceExpression::Vulnerable,
  4.0f, 6.0f,
  BF_USER_ACTION
};
