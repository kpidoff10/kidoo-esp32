#include "behavior_engine.h"
#include "poop_manager.h"
#include "dirt_overlay.h"
#include "behavior_objects.h"
#include "sprites/sprite_heart_24.h"
#include "sprites/sprite_sparkle_26.h"
#include "sprites/sprite_droplet_22.h"
#include "sprites/sprite_microbe_24.h"
#include "sprites/sprite_anger_26.h"
#include "../face_engine.h"
#include "../gotchi_haptic.h"
#include "../../config/gotchi_config.h"
#include <cstdlib>
#include <cstring>
#include <ctime>
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
uint32_t s_persistTimer = 0;             // Sauvegarde periodique des stats
constexpr uint32_t PERSIST_INTERVAL_MS = 60000;  // sauvegarder toutes les 60s

// --- Particules d'ambiance (background discret selon humeur) ---
// Spawn rare (~3-8s entre 2 particules), pas pendant les behaviors actifs
// pour eviter de saturer visuellement.
uint32_t s_ambientTimer = 0;
uint32_t s_nextAmbientIn = 5000;

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
  &BEHAVIOR_THERMOMETER,
  &BEHAVIOR_MEDICINE,
};
constexpr int BEHAVIOR_COUNT = sizeof(ALL_BEHAVIORS) / sizeof(ALL_BEHAVIORS[0]);

// Tick des particules d'ambiance : spawn discret en fond selon l'humeur du gotchi.
// Pas pendant les behaviors actifs qui ont leur propre flux d'objets.
void updateAmbientParticles(uint32_t dtMs) {
  s_ambientTimer += dtMs;
  if (s_ambientTimer < s_nextAmbientIn) return;
  s_ambientTimer = 0;

  // Pas de particules pendant les behaviors qui spawn deja leurs propres sprites
  const char* n = s_current ? s_current->name : "";
  bool busy = (strcmp(n, "happy") == 0 ||
               strcmp(n, "eating") == 0 ||
               strcmp(n, "hungry") == 0 ||
               strcmp(n, "sad") == 0 ||
               strcmp(n, "sick") == 0 ||
               strcmp(n, "dirty") == 0 ||
               strcmp(n, "tantrum") == 0 ||
               strcmp(n, "curious") == 0 ||
               strcmp(n, "play") == 0 ||
               strcmp(n, "sleep") == 0);

  if (busy) {
    s_nextAmbientIn = 4000 + rand() % 3000;
    return;
  }

  // Choix de la particule selon l'humeur dominante
  const SpriteAsset* asset = nullptr;
  uint32_t color = 0;
  uint32_t interval = 5000;

  if (s_stats.irritability > 60) {
    asset = &SPRITE_ANGER_26_ASSET;
    interval = 3000;
  } else if (s_stats.health < 30) {
    asset = &SPRITE_MICROBE_24_ASSET;
    interval = 4000;
  } else if (s_stats.happiness < 25) {
    asset = &SPRITE_DROPLET_22_ASSET;
    interval = 6000;
  } else if (s_stats.happiness > 70) {
    if (rand() % 2) {
      asset = &SPRITE_HEART_24_ASSET;
      color = 0xFF6090;
    } else {
      asset = &SPRITE_SPARKLE_26_ASSET;
    }
    interval = 4500;
  } else {
    // Humeur neutre → pas de particule
    s_nextAmbientIn = 6000 + rand() % 3000;
    return;
  }

  // Spawn discret sur les cotes (pas devant les yeux), monte doucement
  bool leftSide = (rand() % 2) == 0;
  float sx = leftSide ? (40.0f + rand() % 50)
                       : (376.0f + rand() % 50);
  float sy = 320.0f + rand() % 60;
  float vx = leftSide ? (rand() % 10) / 1000.0f
                       : -((rand() % 10) / 1000.0f);
  float vy = -0.04f - (rand() % 20) / 1000.0f;

  BehaviorObjects::spawnSprite(*asset, color, sx, sy, vx, vy, 0, 0, false, 3500);

  // Variation aleatoire entre les spawns pour pas que ce soit metronomique
  s_nextAmbientIn = interval + rand() % 2000;
}

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
    case Need::Bored: {
      // Ne joue a la balle QUE s'il est vraiment en forme (pas fatigue, pas
      // affame, pas malade). Sinon il devient juste curieux (moins couteux).
      bool fitToPlay = (s_stats.energy > 50 &&
                        s_stats.hunger > 30 &&
                        s_stats.health > 50);
      if (fitToPlay && (rand() % 2)) return &BEHAVIOR_PLAY_BALL;
      return &BEHAVIOR_CURIOUS;
    }
    case Need::Excited:   return &BEHAVIOR_HAPPY;
    // Pas de besoin → reste idle. Curiosity est declenchee uniquement par
    // processRandomEvents (5% / 30s) pour ne pas saturer.
    case Need::None:      return &BEHAVIOR_IDLE;
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

  // Pic de curiosite : seulement si idle depuis longtemps (>60s) pour ne pas
  // interrompre les scenes idle qui mettent un moment a se declencher.
  if (roll < 15 && s_current == &BEHAVIOR_IDLE && s_elapsed > 60000) {
    switchTo(&BEHAVIOR_CURIOUS);
    return;
  }

  // Petites variations aleatoires (rare) — utilise des plages exclusives
  // pour ne pas se cumuler avec tantrum/curious deja gerees plus haut.
  // 3% chance de baisse legere d'hygiene
  if (roll >= 15 && roll < 18) {
    s_stats.hygiene -= 1;
    s_stats.clamp();
  }
  // 3% chance de petit pic de faim
  if (roll >= 18 && roll < 21) {
    s_stats.hunger -= 1;
    s_stats.clamp();
  }
}

} // namespace

// Defini dans behavior_eating.cpp — setter la nourriture avant switchTo
extern void eatingSetFood(const char* food);

// Declares globales — definies dans behavior_play_ball.cpp
extern void playBallLaunchFrom(float fromX, float dir);
extern int playBallGetId();
#define BALL_ID() (::playBallGetId())

// Definies dans behavior_thermometer.cpp
extern void thermometerRequestRemove();
extern bool thermometerIsRemoving();

// Défini dans behavior_eating.cpp (doit être déclaré au scope global, pas dans le namespace)
extern bool s_retiring;

// Défini dans behavior_idle.cpp
extern bool idleTriggerScene(int num);

namespace BehaviorEngine {

void init() {
  BehaviorObjects::init();
  PoopManager::init();
  s_stats = BehaviorStats();
  s_statsLogTimer = 0;
  s_randomEventTimer = 0;
  s_persistTimer = 0;

  // Restaurer les stats persistees + appliquer le decay du temps offline
  GotchiStatsConfig saved = GotchiConfigManager::getStats();
  if (saved.valid) {
    s_stats.hunger       = saved.hunger;
    s_stats.energy       = saved.energy;
    s_stats.happiness    = saved.happiness;
    s_stats.health       = saved.health;
    s_stats.hygiene      = saved.hygiene;
    s_stats.boredom      = saved.boredom;
    s_stats.irritability = saved.irritability;
    s_stats.ageMinutes   = saved.ageMinutes;

    // Decay retroactif : applique le temps ecoule depuis la derniere sauvegarde
    // (RTC requis — si lastSavedAt == 0 ou RTC pas valide, on saute)
    time_t now = time(nullptr);
    if (saved.lastSavedAt > 0 && now > (time_t)saved.lastSavedAt) {
      uint32_t offlineSec = (uint32_t)(now - (time_t)saved.lastSavedAt);
      // Cap a 7 jours pour eviter qu'un gotchi laisse hors ligne longtemps
      // soit completement detruit en revenant
      if (offlineSec > 7 * 86400) offlineSec = 7 * 86400;
      // Simule en gros morceaux de 60s pour eviter de boucler des millions de fois
      uint32_t bigStep = 60000;
      uint32_t remaining = offlineSec * 1000;
      while (remaining > 0) {
        uint32_t step = remaining > bigStep ? bigStep : remaining;
        s_stats.decay(step);
        remaining -= step;
      }
      s_stats.ageMinutes += offlineSec / 60;
      Serial.printf("[GOTCHI] Decay offline applique: %lus (~%lum)\n",
                    offlineSec, offlineSec / 60);
    }
    s_stats.clamp();
  }

  switchTo(&BEHAVIOR_IDLE);
}

static void persistStats() {
  GotchiStatsConfig cfg;
  GotchiConfigManager::initDefault(&cfg);
  cfg.hunger       = s_stats.hunger;
  cfg.energy       = s_stats.energy;
  cfg.happiness    = s_stats.happiness;
  cfg.health       = s_stats.health;
  cfg.hygiene      = s_stats.hygiene;
  cfg.boredom      = s_stats.boredom;
  cfg.irritability = s_stats.irritability;
  cfg.ageMinutes   = s_stats.ageMinutes;
  GotchiConfigManager::saveStats(cfg);
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

  // Sauvegarde periodique des stats sur SD
  s_persistTimer += dtMs;
  if (s_persistTimer >= PERSIST_INTERVAL_MS) {
    s_persistTimer = 0;
    persistStats();
  }

  // Particules d'ambiance (humeur passive)
  updateAmbientParticles(dtMs);
  PoopManager::update(dtMs);
  DirtOverlay::update(dtMs);
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

// Tap localise : reactions visuelles tactiles qui s'executent TOUJOURS,
// meme pendant une scene idle, un behavior, un clin d'oeil precedent, etc.
// Utilise les API *Forced qui posent un lock pour empecher les behaviors
// de l'ecraser a la frame suivante. La reaction dure ~600-900ms puis le
// behavior reprend normalement.
void onTouchAt(int16_t x, int16_t y) {
  // Tester d'abord si on touche un caca
  if (PoopManager::onTouchAt(x, y)) return;

  // Coordonnees ecran 466x466 — centres issus de face_renderer.cpp
  constexpr int16_t SCR_W = 466;
  constexpr int16_t LEFT_EYE_CX  = 138;
  constexpr int16_t RIGHT_EYE_CX = 328;
  constexpr int16_t EYE_CY       = 233;
  constexpr int16_t EYE_RADIUS   = 80;
  constexpr int16_t MOUTH_CX     = 233;
  constexpr int16_t MOUTH_CY     = 318;
  constexpr int16_t MOUTH_RX     = 60;
  constexpr int16_t MOUTH_RY     = 45;
  constexpr int16_t TICKLE_Y     = 380;
  constexpr int16_t FOREHEAD_Y   = 130;

  int16_t dxL = x - LEFT_EYE_CX,  dyL = y - EYE_CY;
  int16_t dxR = x - RIGHT_EYE_CX, dyR = y - EYE_CY;
  int32_t distL2 = (int32_t)dxL * dxL + (int32_t)dyL * dyL;
  int32_t distR2 = (int32_t)dxR * dxR + (int32_t)dyR * dyR;

  // Sleep a sa propre state machine complexe (peek/refuse/wake/grumpy) :
  // on ne veut pas forcer un clin d'oeil par dessus, on delegue.
  const char* curName = s_current ? s_current->name : "";
  if (strcmp(curName, "sleep") == 0) {
    onTouch();
    return;
  }

  // --- Reaction tactile : TOUJOURS jouee meme en plein behavior/scene ---
  bool reacted = false;

  if (distL2 < EYE_RADIUS * EYE_RADIUS && distL2 <= distR2) {
    // Oeil gauche : clin gauche + regard gauche force ~700ms
    FaceEngine::blinkLeft();
    FaceEngine::lookAtForced(-0.4f, 0, 700);
    reacted = true;
  } else if (distR2 < EYE_RADIUS * EYE_RADIUS) {
    // Oeil droit : clin droit + regard droit force ~700ms
    FaceEngine::blinkRight();
    FaceEngine::lookAtForced(0.4f, 0, 700);
    reacted = true;
  } else if (abs(x - MOUTH_CX) < MOUTH_RX && abs(y - MOUTH_CY) < MOUTH_RY) {
    // Bouche : Happy force 900ms
    FaceEngine::setExpressionForced(FaceExpression::Happy, 900);
    s_stats.mouthState = 0.7f;
    s_stats.happiness += 3;
    reacted = true;
  } else if (y > TICKLE_Y) {
    // Chatouilles : Excited force + trauma + blink + stats
    FaceEngine::setExpressionForced(FaceExpression::Excited, 900);
    FaceEngine::trauma(0.6f, 0);
    FaceEngine::blink();
    s_stats.happiness += 8;
    s_stats.excitement += 15;
    s_stats.boredom -= 10;
    reacted = true;
  } else if (y < FOREHEAD_Y) {
    // Front : Confused + regard haut force
    FaceEngine::setExpressionForced(FaceExpression::Confused, 800);
    FaceEngine::lookAtForced(0, -0.5f, 800);
    reacted = true;
  } else if (x < 80) {
    FaceEngine::setExpressionForced(FaceExpression::Suspicious, 800);
    FaceEngine::lookAtForced(-0.6f, 0, 800);
    reacted = true;
  } else if (x > SCR_W - 80) {
    FaceEngine::setExpressionForced(FaceExpression::Suspicious, 800);
    FaceEngine::lookAtForced(0.6f, 0, 800);
    reacted = true;
  }

  // Conserve la logique habituelle (stats globales, callbacks onTouch des
  // behaviors qui veulent reagir : happy/sad/hungry/...).
  // Si le behavior essaie de setExpression en reaction, le lock bloque
  // pendant la duree de notre reaction tactile. Apres expiration du lock,
  // son update normal reprendra sans probleme.
  (void)reacted;
  onTouch();
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

// Vérifier si le gotchi dort et refuse l'action (énergie < 50%)
// Retourne true si l'action est refusée (le gotchi fait non de la tête)
static bool isSleepingAndRefuse() {
  if (s_current != &BEHAVIOR_SLEEP) return false;
  if (s_stats.energy >= 50.0f) return false;  // Assez reposé → accepte
  // Trop fatigué → refuse (petit mouvement de tête "non")
  Serial.println("[BEHAVIOR] Trop fatigué, refuse l'action");
  FaceEngine::setExpression(FaceExpression::Suspicious);
  FaceEngine::lookAtForced(-0.5f, 0, 200);
  return true;
}

void feed(const char* food) {
  if (isSleepingAndRefuse()) return;
  static uint8_t s_refuseCount = 0;
  if (s_stats.hunger >= 90.0f) {
    s_refuseCount++;
    if (s_refuseCount >= 3) {
      Serial.printf("[BEHAVIOR] Enerve: insiste trop! (hunger=%.0f, x%d)\n", s_stats.hunger, s_refuseCount);
      FaceEngine::setExpression(FaceExpression::Angry);
      FaceEngine::shake(FaceEngine::GestureSpeed::Fast);
      s_stats.irritability += 10;
      s_stats.happiness -= 5;
      s_stats.clamp();
    } else {
      Serial.printf("[BEHAVIOR] Refuse de manger: plus faim (hunger=%.0f)\n", s_stats.hunger);
      FaceEngine::setExpression(FaceExpression::Suspicious);
      FaceEngine::shake(FaceEngine::GestureSpeed::Slow);
    }
    return;
  }
  s_refuseCount = 0;
  s_stats.feed(food);
  Serial.printf("[BEHAVIOR] Nourri avec %s (hunger=%.0f)\n", food, s_stats.hunger);
  eatingSetFood(food);
  switchTo(&BEHAVIOR_EATING);  // Toujours switch — eating est BF_USER_ACTION
  persistStats();  // Persist apres action user — gros StaticJsonDocument sur la stack
  // Son joué dans behavior_eating.cpp quand la nourriture arrive à la bouche
}

void stopFeeding() {
  if (s_current == &BEHAVIOR_EATING) {
    Serial.println("[BEHAVIOR] Retrait du biberon");
    ::s_retiring = true;  // Déclenche l'animation de retrait au lieu de couper net
  }
}

void resetTimer() {
  s_elapsed = 0;
}

void startThermometer() {
  if (isSleepingAndRefuse()) return;
  Serial.println("[BEHAVIOR] Thermometer started");
  switchTo(&BEHAVIOR_THERMOMETER);
}

void stopThermometer() {
  if (s_current != &BEHAVIOR_THERMOMETER) return;
  if (thermometerIsRemoving()) {
    // Animation de retrait terminée → sortie effective
    Serial.println("[BEHAVIOR] Thermometer stopped");
    switchTo(chooseBehavior());
  } else {
    // Déclencher l'animation de retrait
    Serial.println("[BEHAVIOR] Thermometer removing...");
    thermometerRequestRemove();
  }
}

void giveMedicine() {
  if (isSleepingAndRefuse()) return;
  Serial.println("[BEHAVIOR] Medicine given");
  switchTo(&BEHAVIOR_MEDICINE);
}

void heal() {
  s_stats.heal();
  Serial.printf("[BEHAVIOR] Soigné (health=%.0f)\n", s_stats.health);
  if (s_autoMode && s_current == &BEHAVIOR_SICK) {
    switchTo(chooseBehavior());
  }
  persistStats();
}

void clean() {
  s_stats.clean();
  PoopManager::cleanAll();
  Serial.printf("[BEHAVIOR] Nettoyé (hygiene=%.0f)\n", s_stats.hygiene);
  if (s_autoMode && s_current == &BEHAVIOR_DIRTY) {
    switchTo(chooseBehavior());
  }
  persistStats();
}

void sleep() {
  if (s_current == &BEHAVIOR_SLEEP) {
    Serial.println("[BEHAVIOR] Deja endormi");
    return;
  }
  Serial.println("[BEHAVIOR] Dodo (tag)");
  switchTo(&BEHAVIOR_SLEEP);
}

void readBook() {
  if (isSleepingAndRefuse()) return;
  Serial.println("[BEHAVIOR] Lecture (tag)");
  switchTo(&BEHAVIOR_IDLE);
  // Forcer la scene Reading (scene 20) — scope global
  ::idleTriggerScene(20);
}

} // namespace BehaviorEngine
