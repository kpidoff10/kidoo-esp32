#include "behavior_engine.h"
#include "behavior_objects.h"
#include "../face_engine.h"
#include <cstdlib>
#include <cstring>
#include <Arduino.h>

namespace {

BehaviorStats s_stats;
const Behavior* s_current = nullptr;
uint32_t s_elapsed = 0;
bool s_autoMode = true;
Need s_currentNeed = Need::None;

// Life clock
uint32_t s_statsLogTimer = 0;
uint32_t s_randomEventTimer = 0;

// Table de tous les behaviors
const Behavior* ALL_BEHAVIORS[] = {
  &BEHAVIOR_IDLE,
  &BEHAVIOR_PLAY_BALL,
  &BEHAVIOR_SLEEP,
  &BEHAVIOR_SAD,
  &BEHAVIOR_HAPPY,
  &BEHAVIOR_HUNGRY,
  &BEHAVIOR_EATING,
  &BEHAVIOR_SICK,
  &BEHAVIOR_DIRTY,
  &BEHAVIOR_LONELY,
  &BEHAVIOR_CURIOUS,
  &BEHAVIOR_TANTRUM,
};
constexpr int BEHAVIOR_COUNT = sizeof(ALL_BEHAVIORS) / sizeof(ALL_BEHAVIORS[0]);

void switchTo(const Behavior* next) {
  if (s_current && s_current->onExit) s_current->onExit();
  BehaviorObjects::destroyAll();

  s_stats.touchCount = 0;
  s_current = next;
  s_elapsed = 0;

  if (s_current) {
    FaceEngine::setExpression(s_current->defaultExpression);
    if (s_current->onEnter) s_current->onEnter();
    Serial.printf("[BEHAVIOR] → %s (need=%s)\n", s_current->name, needToString(s_currentNeed));
  }
}

const Behavior* needToBehavior(Need need) {
  switch (need) {
    case Need::Sick:      return &BEHAVIOR_SICK;
    case Need::Exhausted: return &BEHAVIOR_SLEEP;
    case Need::Hungry:    return &BEHAVIOR_HUNGRY;
    case Need::Dirty:     return &BEHAVIOR_DIRTY;
    case Need::Unhappy:   return &BEHAVIOR_SAD;
    case Need::Lonely:    return &BEHAVIOR_LONELY;
    case Need::Bored:     return (rand() % 2) ? &BEHAVIOR_PLAY_BALL : &BEHAVIOR_CURIOUS;
    case Need::Excited:   return &BEHAVIOR_HAPPY;
    case Need::None:      return (rand() % 5 == 0) ? &BEHAVIOR_CURIOUS : &BEHAVIOR_IDLE;
  }
  return &BEHAVIOR_IDLE;
}

const Behavior* chooseBehavior() {
  s_currentNeed = getMostUrgentNeed(s_stats);
  return needToBehavior(s_currentNeed);
}

void processRandomEvents(uint32_t dtMs) {
  s_randomEventTimer += dtMs;
  if (s_randomEventTimer < 30000) return; // Toutes les 30s
  s_randomEventTimer = 0;

  int roll = rand() % 100;

  // 10% chance de caprice si malheureux
  if (roll < 10 && s_stats.happiness < 50 && s_current != &BEHAVIOR_TANTRUM) {
    switchTo(&BEHAVIOR_TANTRUM);
    return;
  }

  // 5% chance de pic de curiosité
  if (roll < 15 && s_current == &BEHAVIOR_IDLE) {
    switchTo(&BEHAVIOR_CURIOUS);
    return;
  }

  // 8% chance de baisse soudaine d'hygiène
  if (roll < 23) {
    s_stats.hygiene -= 8;
    s_stats.clamp();
  }

  // 5% chance de petit pic de faim
  if (roll < 28) {
    s_stats.hunger -= 5;
    s_stats.clamp();
  }
}

} // namespace

namespace BehaviorEngine {

void init() {
  BehaviorObjects::init();
  s_stats = BehaviorStats();
  s_statsLogTimer = 0;
  s_randomEventTimer = 0;
  switchTo(&BEHAVIOR_IDLE);
}

void update(uint32_t dtMs) {
  // Stats decay naturel + interactions + bouche
  s_stats.decay(dtMs);

  // Passer le mouthState au face engine
  FaceEngine::setMouthState(s_stats.mouthState);

  // Update objets visuels
  BehaviorObjects::update(dtMs);

  // Update le behavior actif
  if (s_current && s_current->onUpdate) {
    s_current->onUpdate(dtMs);
  }

  s_elapsed += dtMs;
  float elapsedSec = s_elapsed / 1000.0f;

  // Les yeux suivent l'objet traqué
  float lx, ly;
  if (BehaviorObjects::getLookTarget(lx, ly)) {
    FaceEngine::lookAt(lx, ly);
  }

  // Transition automatique
  if (s_autoMode && s_current) {
    bool forceEnd = (s_current->maxDurationSec > 0 && elapsedSec >= s_current->maxDurationSec);
    bool canEnd = (elapsedSec >= s_current->minDurationSec);

    if (forceEnd || canEnd) {
      const Behavior* next = chooseBehavior();
      if (next != s_current || forceEnd) {
        switchTo(next);
      }
    }
  }

  // Événements aléatoires
  if (s_autoMode) {
    processRandomEvents(dtMs);
  }

  // Log stats périodique
  s_statsLogTimer += dtMs;
  if (s_statsLogTimer >= 30000) {
    s_statsLogTimer = 0;
    s_stats.printStats(s_current ? s_current->name : "none");
  }
}

void forceState(const char* name) {
  for (int i = 0; i < BEHAVIOR_COUNT; i++) {
    if (strcmp(ALL_BEHAVIORS[i]->name, name) == 0) {
      s_autoMode = false;
      s_currentNeed = Need::None;
      switchTo(ALL_BEHAVIORS[i]);
      return;
    }
  }
  Serial.printf("[BEHAVIOR] Inconnu: %s\n", name);
}

void setAutoMode(bool enabled) {
  s_autoMode = enabled;
  if (enabled) {
    Serial.println("[BEHAVIOR] Mode auto");
    switchTo(chooseBehavior());
  }
}

bool isAutoMode() { return s_autoMode; }

BehaviorStats& getStats() { return s_stats; }

const char* getCurrentBehavior() {
  return s_current ? s_current->name : "none";
}

Need getCurrentNeed() { return s_currentNeed; }

void onTouch() {
  s_stats.touchCount++;
  s_stats.lastTouchAt = millis();
  // Le behavior actif gere en priorite
  if (s_current && s_current->onTouch && s_current->onTouch()) return;
  // Fallback generique
  s_stats.happiness += 5;
  s_stats.excitement += 10;
  s_stats.boredom -= 8;
  s_stats.clamp();
}

void onShake() {
  s_stats.touchCount++;
  s_stats.lastTouchAt = millis();
  // Le behavior actif gere en priorite
  if (s_current && s_current->onShake && s_current->onShake()) return;
  // Fallback generique
  s_stats.excitement += 20;
  s_stats.mouthState = -0.5f;
  s_stats.clamp();
}

void requestBehavior(const Behavior* behavior) {
  if (behavior) switchTo(behavior);
}

void onSound() {
  s_stats.excitement += 10;
  s_stats.boredom -= 5;
  s_stats.clamp();
}

void feed(const char* food) {
  s_stats.feed(food);
  Serial.printf("[BEHAVIOR] Nourri avec %s (hunger=%.0f)\n", food, s_stats.hunger);
  if (s_autoMode) switchTo(&BEHAVIOR_EATING);
}

void heal() {
  s_stats.heal();
  Serial.printf("[BEHAVIOR] Soigné (health=%.0f)\n", s_stats.health);
  if (s_autoMode && s_current == &BEHAVIOR_SICK) {
    switchTo(chooseBehavior());
  }
}

void clean() {
  s_stats.clean();
  Serial.printf("[BEHAVIOR] Nettoyé (hygiene=%.0f)\n", s_stats.hygiene);
  if (s_autoMode && s_current == &BEHAVIOR_DIRTY) {
    switchTo(chooseBehavior());
  }
}

} // namespace BehaviorEngine
