#ifndef BEHAVIOR_ENGINE_H
#define BEHAVIOR_ENGINE_H

#include <cstdint>
#include "behavior_stats.h"
#include "behavior_needs.h"
#include "../face_config.h"

struct Behavior {
  const char* name;
  void (*onEnter)();
  void (*onUpdate)(uint32_t dtMs);
  void (*onExit)();
  FaceExpression defaultExpression;
  float minDurationSec;
  float maxDurationSec;
};

namespace BehaviorEngine {

void init();
void update(uint32_t dtMs);

void forceState(const char* name);
void setAutoMode(bool enabled);
bool isAutoMode();

BehaviorStats& getStats();
const char* getCurrentBehavior();
Need getCurrentNeed();

// Événements externes
void onTouch();
void onShake();
void onSound();

// Actions utilisateur
void feed(const char* food);
void heal();
void clean();

} // namespace BehaviorEngine

// Behaviors disponibles
extern const Behavior BEHAVIOR_IDLE;
extern const Behavior BEHAVIOR_PLAY_BALL;
extern const Behavior BEHAVIOR_SLEEP;
extern const Behavior BEHAVIOR_SAD;
extern const Behavior BEHAVIOR_HAPPY;
extern const Behavior BEHAVIOR_HUNGRY;
extern const Behavior BEHAVIOR_EATING;
extern const Behavior BEHAVIOR_SICK;
extern const Behavior BEHAVIOR_DIRTY;
extern const Behavior BEHAVIOR_LONELY;
extern const Behavior BEHAVIOR_CURIOUS;
extern const Behavior BEHAVIOR_TANTRUM;

#endif
