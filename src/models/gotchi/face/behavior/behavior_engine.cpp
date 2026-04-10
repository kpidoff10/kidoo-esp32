#include "behavior_engine.h"
#include "behavior_objects.h"
#include "../face_engine.h"
#include "../gotchi_haptic.h"
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

  // Pas d'events random pendant une action user
  if (s_current && (s_current->flags & BF_USER_ACTION)) return;

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

// Declares globales — definies dans behavior_play_ball.cpp
extern void playBallLaunchFrom(float fromX, float dir);
extern int playBallGetId();
#define BALL_ID() (::playBallGetId())

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
    bool isUserAction = (s_current->flags & BF_USER_ACTION);
    bool forceEnd = (s_current->maxDurationSec > 0 && elapsedSec >= s_current->maxDurationSec);
    bool canEnd = (elapsedSec >= s_current->minDurationSec);

    if (forceEnd || canEnd) {
      const Behavior* next = chooseBehavior();
      // User actions bloquent l'auto-transition SAUF:
      // 1. maxDuration atteint (safety valve)
      // 2. Le next behavior est urgent (sick)
      if (isUserAction && !forceEnd && !(next->flags & BF_URGENT)) {
        // Skip — action user continue
      } else if (next != s_current || forceEnd) {
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

// --- Ball drag (play mode) ---
static bool s_draggingBall = false;
static float s_dragPrevX = 0, s_dragPrevY = 0;  // position N-1
static float s_dragLastX = 0, s_dragLastY = 0;  // position N
static uint32_t s_dragPrevTime = 0;
static uint32_t s_dragLastTime = 0;

void onFingerDown(float x, float y) {
  if (s_current != &BEHAVIOR_PLAY_BALL) return;
  int ballId = BALL_ID();
  if (ballId < 0) {
    // Pas de balle → en creer une sous le doigt
    ::playBallLaunchFrom(x, 0);
    ballId = BALL_ID();
    if (ballId < 0) return;
  }
  BehaviorObjects::hold(ballId, x, y);
  s_draggingBall = true;
  s_dragPrevX = x;
  s_dragPrevY = y;
  s_dragPrevTime = millis();
  s_dragLastX = x;
  s_dragLastY = y;
  s_dragLastTime = millis();
  FaceEngine::setAutoMode(false);  // Stop blink/look aleatoire
  FaceEngine::setExpression(FaceExpression::Excited);
  GotchiHaptic::ballCatch();
}

void onFingerMove(float x, float y) {
  if (!s_draggingBall) return;
  int ballId = BALL_ID();
  if (ballId < 0) { s_draggingBall = false; return; }
  BehaviorObjects::hold(ballId, x, y);
  uint32_t now = millis();
  // Garder les 2 dernieres positions pour calculer la velocite au lacher
  s_dragPrevX = s_dragLastX;
  s_dragPrevY = s_dragLastY;
  s_dragPrevTime = s_dragLastTime;
  s_dragLastX = x;
  s_dragLastY = y;
  s_dragLastTime = now;
}

void onFingerUp(float x, float y, float vx, float vy) {
  if (!s_draggingBall) return;
  s_draggingBall = false;
  FaceEngine::setAutoMode(true);
  int ballId = BALL_ID();
  if (ballId < 0) return;

  // Calculer velocite depuis les dernieres positions (pas le total)
  uint32_t dt = s_dragLastTime - s_dragPrevTime;
  float throwVx = 0, throwVy = 0;
  if (dt > 0 && dt < 200) {  // Seulement si mouvement recent (<200ms)
    float scale = 1.0f / (float)dt;  // px/ms
    throwVx = (s_dragLastX - s_dragPrevX) * scale * 0.4f;
    throwVy = (s_dragLastY - s_dragPrevY) * scale * 0.4f;
    // Clamp pour pas que ca parte trop vite
    if (throwVx > 0.3f) throwVx = 0.3f;
    if (throwVx < -0.3f) throwVx = -0.3f;
    if (throwVy > 0.3f) throwVy = 0.3f;
    if (throwVy < -0.3f) throwVy = -0.3f;
  }

  BehaviorObjects::release(ballId, throwVx, throwVy);
  FaceEngine::setExpression(FaceExpression::Amazed);
  s_stats.happiness += 3;
  s_stats.excitement += 5;
  s_stats.clamp();
}

void onSwipe(float startX, float startY, float dirX, float dirY) {
  // Pendant play → lancer la balle
  if (s_current == &BEHAVIOR_PLAY_BALL) {
    float ballX = startX;
    if (ballX < 60) ballX = 60;
    if (ballX > 406) ballX = 406;
    ::playBallLaunchFrom(ballX, dirX);
    s_stats.happiness += 3;
    s_stats.excitement += 8;
    s_stats.clamp();
    return;
  }
  // Hors play : un swipe = juste un touch
  onTouch();
}

void onPet() {
  // Caresse : toujours douce et positive, reaction selon behavior
  const char* name = s_current ? s_current->name : "";

  if (strcmp(name, "sleep") == 0) {
    // Dort : sourit doucement sans se reveiller
    s_stats.happiness += 2;
    s_stats.mouthState = -0.2f; // Petit sourire endormi
  } else if (strcmp(name, "tantrum") == 0) {
    // Calme la colere doucement
    s_stats.irritability -= 3;
    s_stats.happiness += 2;
    if (s_stats.irritability < 20) {
      switchTo(&BEHAVIOR_SAD); // Colere fond en tristesse
    }
  } else if (strcmp(name, "sad") == 0) {
    // Reconfort profond
    s_stats.happiness += 5;
    FaceEngine::setExpression(FaceExpression::Vulnerable);
    FaceEngine::nod(FaceEngine::GestureSpeed::Slow);
  } else if (strcmp(name, "lonely") == 0) {
    // Enorme soulagement
    s_stats.happiness += 8;
    s_stats.boredom -= 10;
    FaceEngine::setExpression(FaceExpression::Happy);
  } else if (strcmp(name, "sick") == 0) {
    // Reconfort doux
    s_stats.happiness += 3;
    s_stats.health += 1;
  } else if (strcmp(name, "hungry") == 0) {
    // Apprecie mais veut manger
    s_stats.happiness += 1;
  } else {
    // Default : content
    s_stats.happiness += 3;
    s_stats.boredom -= 3;
    s_stats.mouthState = 0.5f; // Sourire
  }
  // Toute caresse calme un peu
  s_stats.irritability -= 1;
  s_stats.clamp();
  Serial.printf("[BEHAVIOR] Pet! (happiness=%.0f irrit=%.0f)\n", s_stats.happiness, s_stats.irritability);
}

void onSound() {
  s_stats.excitement += 10;
  s_stats.boredom -= 5;
  s_stats.clamp();
}

bool tryPlay() {
  // Le gotchi peut refuser de jouer selon son etat
  if (s_stats.health < 30) {
    // Malade → refuse
    FaceEngine::setExpression(FaceExpression::Vulnerable);
    FaceEngine::shake(FaceEngine::GestureSpeed::Slow);
    s_stats.mouthState = -0.3f;
    Serial.println("[BEHAVIOR] Refuse de jouer: malade");
    return false;
  }
  if (s_stats.energy < 20) {
    // Epuise → refuse
    FaceEngine::setExpression(FaceExpression::Tired);
    FaceEngine::shake(FaceEngine::GestureSpeed::Slow);
    s_stats.mouthState = -0.2f;
    Serial.println("[BEHAVIOR] Refuse de jouer: fatigue");
    return false;
  }
  if (s_stats.hunger < 15) {
    // Affame → refuse, veut manger
    FaceEngine::setExpression(FaceExpression::Pleading);
    FaceEngine::shake(FaceEngine::GestureSpeed::Normal);
    s_stats.mouthState = -0.6f;
    Serial.println("[BEHAVIOR] Refuse de jouer: faim");
    return false;
  }
  if (s_stats.happiness < 15) {
    // Trop triste → refuse
    FaceEngine::setExpression(FaceExpression::Sad);
    FaceEngine::shake(FaceEngine::GestureSpeed::Slow);
    Serial.println("[BEHAVIOR] Refuse de jouer: triste");
    return false;
  }
  // OK → lance le jeu
  s_autoMode = true;  // S'assurer que l'auto est actif pour les transitions apres le jeu
  switchTo(&BEHAVIOR_PLAY_BALL);
  Serial.println("[BEHAVIOR] Jouer!");
  return true;
}

void feed(const char* food) {
  s_stats.feed(food);
  Serial.printf("[BEHAVIOR] Nourri avec %s (hunger=%.0f)\n", food, s_stats.hunger);
  switchTo(&BEHAVIOR_EATING);  // Toujours switch — eating est BF_USER_ACTION
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
