#include "../behavior_engine.h"
#include "../behavior_objects.h"
#include "../../face_engine.h"
#include "../../overlay/face_overlay_layer.h"
#include <cstdlib>
#include <cmath>

namespace {

// --- Drool state ---
uint32_t s_phase = 0;
float s_breathAngle = 0;
float s_droolLength = 0;
float s_droolRetract = 0;
uint32_t s_droolPause = 0;
enum DroolState { DS_PAUSE, DS_GROWING, DS_FALLING, DS_RETRACTING };
DroolState s_droolState = DS_PAUSE;

// --- Touch wake state machine ---
enum SleepTouch { ST_ASLEEP, ST_PEEK, ST_REFUSE, ST_WAKING, ST_GRUMPY };
SleepTouch s_touchState = ST_ASLEEP;
uint32_t s_touchTimer = 0;
uint8_t  s_tapCount = 0;

void stopDrool(BehaviorStats& stats) {
  s_droolLength = 0;
  s_droolRetract = 0;
  s_droolState = DS_PAUSE;
  stats.droolLength = 0;
  stats.droolRetract = 0;
}

} // namespace

// --- Touch handler ---
static bool sleepOnTouch() {
  auto& stats = BehaviorEngine::getStats();
  s_tapCount++;

  // Grumpy: 3+ taps while very tired
  if (s_tapCount >= 3 && stats.energy <= 30) {
    s_touchState = ST_GRUMPY;
    s_touchTimer = 0;
    stats.irritability += 30;
    stats.happiness -= 15;
    stats.clamp();
    FaceOverlayLayer::setSleepZzz(false);
    stopDrool(stats);
    FaceEngine::setExpression(FaceExpression::Furious);
    stats.mouthState = -0.6f;
    // Transition vers tantrum apres un court delai (geree dans onUpdate)
    return true;
  }

  if (s_touchState == ST_ASLEEP || s_touchState == ST_REFUSE) {
    s_touchState = ST_PEEK;
    s_touchTimer = 0;
    FaceOverlayLayer::setSleepZzz(false);
    stopDrool(stats);
    // Ouvre un oeil (expression Tired = yeux mi-clos)
    FaceEngine::setExpression(FaceExpression::Tired);
    stats.mouthState = 0.0f;
    return true;
  }

  if (s_touchState == ST_PEEK) {
    // Double tap pendant peek = force wake si energie OK
    if (stats.energy > 30) {
      s_touchState = ST_WAKING;
      s_touchTimer = 0;
    }
    return true;
  }

  return true;
}

static void onEnter() {
  FaceOverlayLayer::setSleepZzz(false);
  FaceEngine::setAutoMode(false);
  FaceEngine::setExpression(FaceExpression::Tired);
  s_phase = 0;
  s_breathAngle = 0;
  s_droolLength = 0;
  s_droolRetract = 0;
  s_droolPause = 3000 + rand() % 3000;
  s_droolState = DS_PAUSE;
  s_touchState = ST_ASLEEP;
  s_touchTimer = 0;
  s_tapCount = 0;
  BehaviorEngine::getStats().droolLength = 0;
}

static void onUpdate(uint32_t dtMs) {
  auto& stats = BehaviorEngine::getStats();
  float sec = dtMs / 1000.0f;

  // === Touch state machine (prioritaire) ===
  if (s_touchState != ST_ASLEEP) {
    s_touchTimer += dtMs;

    switch (s_touchState) {
      case ST_PEEK:
        // Un oeil ouvert pendant 1.5s
        FaceEngine::lookAt(0, 0);
        if (s_touchTimer > 1500) {
          if (stats.energy > 30) {
            // Assez d'energie → se reveille
            s_touchState = ST_WAKING;
            s_touchTimer = 0;
          } else {
            // Trop fatigue → refuse
            s_touchState = ST_REFUSE;
            s_touchTimer = 0;
            FaceEngine::shake(FaceEngine::GestureSpeed::Slow);
            FaceEngine::setExpression(FaceExpression::Annoyed);
            stats.mouthState = -0.2f;
          }
        }
        break;

      case ST_REFUSE:
        // Shake "non" pendant 2s puis re-dort
        if (s_touchTimer > 2000) {
          FaceEngine::setExpression(FaceExpression::Asleep);
          FaceOverlayLayer::setSleepZzz(true);
          stats.mouthState = -0.35f;
          s_touchState = ST_ASLEEP;
          s_touchTimer = 0;
        }
        break;

      case ST_WAKING:
        // Reveil progressif : blinks + transition
        if (s_touchTimer < 400) {
          FaceEngine::setExpression(FaceExpression::Tired);
          stats.mouthState = -0.5f; // Baillement
        } else if (s_touchTimer < 800) {
          FaceEngine::blink();
        } else if (s_touchTimer < 1200) {
          FaceEngine::setExpression(FaceExpression::Normal);
          stats.mouthState = 0.1f;
        } else {
          // Reveille → retour idle
          stats.mouthState = 0.0f;
          BehaviorEngine::requestBehavior(&BEHAVIOR_IDLE);
        }
        break;

      case ST_GRUMPY:
        // Furieux 1s puis tantrum
        if (s_touchTimer > 1000) {
          stats.happiness -= 10;
          stats.clamp();
          BehaviorEngine::requestBehavior(&BEHAVIOR_TANTRUM);
        }
        break;

      default:
        break;
    }

    // Pendant les etats de reveil, on recupere quand meme un peu d'energie
    stats.addEnergy(1.0f, dtMs);
    return; // Pas de sleep normal pendant la state machine
  }

  // === Sleep normal ===
  s_phase += dtMs;
  stats.addEnergy(3.0f, dtMs);

  // Respiration
  s_breathAngle += sec * 1.6f;
  float breath = sinf(s_breathAngle);

  float targetMouth = -0.35f - breath * 0.12f;
  stats.mouthState = roundf(targetMouth * 20.0f) / 20.0f;

  float eyeBreath = breath * 0.05f;
  FaceEngine::lookAt(0, 0.1f + eyeBreath);

  // Phases d'expression
  if (s_phase < 2000) {
    // Tired
  } else if (s_phase < 4000 && s_phase - dtMs < 2000) {
    FaceEngine::setExpression(FaceExpression::Asleep);
  }

  // Animation bave (apres endormissement)
  if (s_phase > 4000) {
    switch (s_droolState) {
      case DS_PAUSE:
        s_droolLength = 0;
        s_droolRetract = 0;
        if (s_droolPause > dtMs) s_droolPause -= dtMs;
        else { s_droolPause = 0; s_droolState = DS_GROWING; }
        break;
      case DS_GROWING:
        s_droolLength += sec * 12.0f;
        s_droolRetract = 0;
        if (s_droolLength > 25.0f) s_droolState = DS_FALLING;
        break;
      case DS_FALLING:
        s_droolLength += sec * 50.0f;
        if (s_droolLength > 40.0f) s_droolState = DS_RETRACTING;
        break;
      case DS_RETRACTING:
        s_droolLength += sec * 30.0f;
        s_droolRetract += sec * 40.0f;
        if (s_droolRetract >= s_droolLength) {
          s_droolLength = 0;
          s_droolRetract = 0;
          s_droolState = DS_PAUSE;
          s_droolPause = 4000 + rand() % 5000;
        }
        break;
    }
    stats.droolLength = s_droolLength;
    stats.droolRetract = s_droolRetract;
  }

  FaceOverlayLayer::setSleepZzz(s_phase > 4000);
}

static void onExit() {
  FaceOverlayLayer::setSleepZzz(false);
  FaceEngine::setExpression(FaceExpression::Tired);
  auto& stats = BehaviorEngine::getStats();
  stats.mouthState = 0;
  stats.droolLength = 0;
  stats.droolRetract = 0;
}

const Behavior BEHAVIOR_SLEEP = {
  "sleep", onEnter, onUpdate, onExit, sleepOnTouch, nullptr,
  FaceExpression::Asleep,
  10.0f,
  30.0f
};
