#ifndef BEHAVIOR_ENGINE_H
#define BEHAVIOR_ENGINE_H

#include <cstdint>
#include "behavior_stats.h"
#include "behavior_needs.h"
#include "../face_config.h"

// Behavior flags (bitmask)
constexpr uint8_t BF_NONE        = 0;
constexpr uint8_t BF_USER_ACTION = 1 << 0;  // Protege des auto-transitions (play, eating)
constexpr uint8_t BF_URGENT      = 1 << 1;  // Peut interrompre une action user (sick)

struct Behavior {
  const char* name;
  void (*onEnter)();
  void (*onUpdate)(uint32_t dtMs);
  void (*onExit)();
  bool (*onTouch)();    // true = handled, nullptr = fallback
  bool (*onShake)();    // true = handled, nullptr = fallback
  FaceExpression defaultExpression;
  float minDurationSec;
  float maxDurationSec;
  uint8_t flags;        // BF_NONE, BF_USER_ACTION, BF_URGENT
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
void onTouchAt(int16_t x, int16_t y);  // Tap localise (zones tactiles : yeux, bouche, chatouilles...)
void onShake();
void onPet();
void onSwipe(float startX, float startY, float dirX, float dirY);
void onFingerDown(float x, float y);
void onFingerMove(float x, float y);
void onFingerUp(float x, float y, float vx, float vy);
void onSound();

// Demande de transition (appelable depuis un behavior callback)
void requestBehavior(const Behavior* behavior);

// Actions utilisateur
bool tryPlay();  // Retourne false si le gotchi refuse de jouer
void feed(const char* food);
void stopFeeding();       // Arrêter le biberon (face feed bottle off)
void startThermometer();  // Prendre la température
void stopThermometer();   // Retirer le thermomètre
void resetTimer();        // Reset le timer du behavior courant (empêche timeout)
void giveMedicine();  // Donner un médicament (animation + heal)
void heal();          // Heal direct sans animation
void clean();
void sleep();             // Endormir le gotchi
void readBook();          // Lire un livre (tag)

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
extern const Behavior BEHAVIOR_THERMOMETER;
extern const Behavior BEHAVIOR_MEDICINE;

#endif
