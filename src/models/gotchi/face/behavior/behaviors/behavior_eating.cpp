#include "../behavior_engine.h"
#include "../behavior_objects.h"
#include "../sprites/sprite_sparkle_26.h"
#include "../sprites/sprite_apple_44.h"
#include "../sprites/sprite_cookie_44.h"
#include "../sprites/sprite_strawberry_44.h"
#include "../sprites/sprite_donut_44.h"
#include "../sprites/sprite_pizza_44.h"
#include "../sprites/sprite_bottle_66.h"
#include "../../face_engine.h"
#include "../../gotchi_haptic.h"
#include "../../../audio/gotchi_speaker_test.h"
#include <cstdlib>
#include <cstring>

// Accessible depuis behavior_engine.cpp (extern)
bool s_retiring = false;

namespace {
uint32_t s_timer = 0;
uint32_t s_chewTimer = 0;
bool s_chewing = false;
uint8_t s_eatPhase = 0;  // 0=regarde, 1=approche, 2=mâche/boit, 3=retrait biberon
int s_foodObjId = -1;
char s_foodName[16] = "apple";
uint8_t s_crumbCount = 0;  // Compteur de miettes/bulles spawned
bool s_isBottle = false;   // Mode biberon (animation différente)
uint32_t s_retireTimer = 0;

// Position de la bouche (doit matcher face_renderer.cpp)
constexpr float MOUTH_X = 233.0f;
constexpr float MOUTH_Y = 318.0f;
// Couleurs de miettes par type de nourriture (RGB565)
constexpr uint32_t CRUMB_COLOR_DEFAULT = 0xD968;  // rouge pomme
// Couleur des bulles du biberon (blanc)
constexpr uint32_t BUBBLE_COLOR = 0xFFFF;

const SpriteAsset* getFoodSprite(const char* food) {
  if (strcmp(food, "bottle") == 0)     return &SPRITE_BOTTLE_66_ASSET;
  if (strcmp(food, "cookie") == 0)     return &SPRITE_COOKIE_44_ASSET;
  if (strcmp(food, "strawberry") == 0) return &SPRITE_STRAWBERRY_44_ASSET;
  if (strcmp(food, "donut") == 0)      return &SPRITE_DONUT_44_ASSET;
  if (strcmp(food, "pizza") == 0)      return &SPRITE_PIZZA_44_ASSET;
  return &SPRITE_APPLE_44_ASSET;  // apple, cake, défaut
}
}

static void onEnter() {
  FaceEngine::setAutoMode(true);
  FaceEngine::setExpression(FaceExpression::Happy);
  FaceEngine::lookAt(0, 0.3f);  // Regarde vers le bas (vers la bouche)
  BehaviorEngine::getStats().mouthState = 0.6f;
  s_timer = 0;
  s_chewTimer = 0;
  s_chewing = false;
  s_crumbCount = 0;
  s_isBottle = (strcmp(s_foodName, "bottle") == 0);
  s_retiring = false;
  s_retireTimer = 0;

  s_eatPhase = 0;
  s_foodObjId = -1;

  // Phase 0 : nourriture apparaît au niveau de la bouche (côté droit), le gotchi la regarde
  const SpriteAsset* food = getFoodSprite(s_foodName);
  s_foodObjId = BehaviorObjects::spawnSprite(*food, 0,
    MOUTH_X + 80.0f, MOUTH_Y - 30.0f, 0, 0, 0, 0, true, 10000);
}

static void onUpdate(uint32_t dtMs) {
  auto& stats = BehaviorEngine::getStats();
  s_timer += dtMs;

  // Phase 0 : regarde la nourriture (0-800ms)
  if (s_eatPhase == 0 && s_timer >= 800) {
    s_eatPhase = 1;
    // Phase 1 : nourriture glisse vers la bouche (depuis la droite)
    if (s_foodObjId >= 0) {
      BehaviorObjects::destroy(s_foodObjId);
    }
    const SpriteAsset* food = getFoodSprite(s_foodName);
    s_foodObjId = BehaviorObjects::spawnSprite(*food, 0,
      MOUTH_X + 80.0f, MOUTH_Y - 30.0f, -0.07f, 0.02f, 0, 0, true,
      s_isBottle ? 8000 : 3000);
    FaceEngine::setExpression(FaceExpression::Excited);
    stats.mouthState = -0.5f;  // Bouche ouverte
  }

  // Phase 1 : la nourriture arrive à la bouche
  if (s_eatPhase == 1 && s_timer >= 2000) {
    s_eatPhase = 2;

    if (s_isBottle) {
      // --- BIBERON : le sprite reste collé à la bouche, durée infinie ---
      if (s_foodObjId >= 0) {
        BehaviorObjects::destroy(s_foodObjId);
      }
      const SpriteAsset* food = getFoodSprite(s_foodName);
      s_foodObjId = BehaviorObjects::spawnSprite(*food, 0,
        MOUTH_X, MOUTH_Y + 30.0f, 0, 0, 0, 0, false, 0);  // lifetime 0 = infini
      stats.mouthState = -0.3f;  // Bouche ouverte (tète)
      FaceEngine::setExpression(FaceExpression::Happy);
    } else {
      // --- SOLIDE : détruire le sprite (mangé !) ---
      if (s_foodObjId >= 0) {
        BehaviorObjects::destroy(s_foodObjId);
        s_foodObjId = -1;
      }
      // Sparkles de satisfaction — au-dessus des yeux, bien espacées
      for (int i = 0; i < 3; i++) {
        float x = 130.0f + i * 100.0f + (rand() % 30);
        BehaviorObjects::spawnSprite(SPRITE_SPARKLE_26_ASSET, 0,
          x, 150.0f, ((rand() % 20) - 10) / 1000.0f, -0.05f,
          0, 0, false, 2000);
      }
      GotchiHaptic::chew();
      GotchiSpeakerTest::playEatingSound();
      stats.mouthState = 0.5f;  // Bouche fermée, croque !
      FaceEngine::setExpression(FaceExpression::Happy);
    }
    s_crumbCount = 0;
  }

  // Phase 3 : retrait du biberon (glisse vers la droite)
  if (s_eatPhase == 3) {
    s_retireTimer += dtMs;
    BehaviorEngine::resetTimer();

    // Après 1s d'animation de retrait, quitter
    if (s_retireTimer >= 1000) {
      stats.mouthState = 0.5f;  // Sourire satisfait
      FaceEngine::setExpression(FaceExpression::Happy);
      BehaviorEngine::setAutoMode(true);
      BehaviorEngine::requestBehavior(&BEHAVIOR_IDLE);
    }
    return;
  }

  // Phase 2 : mastication (solide) ou tétée (biberon)
  if (s_eatPhase == 2) {
    s_chewTimer += dtMs;

    if (s_isBottle) {
      // Vérifier si le retrait a été demandé
      if (s_retiring) {
        s_eatPhase = 3;
        s_retireTimer = 0;
        // Détruire le biberon collé et en recréer un qui glisse vers la droite
        if (s_foodObjId >= 0) {
          BehaviorObjects::destroy(s_foodObjId);
        }
        const SpriteAsset* food = getFoodSprite(s_foodName);
        s_foodObjId = BehaviorObjects::spawnSprite(*food, 0,
          MOUTH_X, MOUTH_Y + 30.0f, 0.1f, -0.02f, 0, 0, false, 1000);
        stats.mouthState = 0.3f;  // Bouche se ferme
        FaceEngine::setExpression(FaceExpression::Happy);
        FaceEngine::blink();
        return;
      }

      // Empêcher le timeout automatique — le biberon reste jusqu'à retrait
      BehaviorEngine::resetTimer();
      // --- BIBERON : succion douce ---
      if (s_chewTimer > 500) {
        s_chewTimer = 0;
        s_chewing = !s_chewing;
        // Bouche alterne entre légèrement ouverte et fermée (tétée)
        stats.mouthState = s_chewing ? -0.3f : -0.1f;
        // Petites bulles blanches qui montent depuis le biberon
        if (s_crumbCount >= 5) s_crumbCount = 0;  // Cycle les bulles
        float bx = MOUTH_X - 10.0f + (rand() % 20);  // centré sur la bouche
        BehaviorObjects::spawn(ObjectShape::Circle, BUBBLE_COLOR, 3,
          bx, MOUTH_Y + 10.0f, ((rand() % 10) - 5) / 1000.0f, -0.04f,
          0, 0, false, 1200);
        s_crumbCount++;
      }
    } else {
      // --- SOLIDE : mastication + miettes ---
      if (s_chewTimer > 350) {
        s_chewTimer = 0;
        s_chewing = !s_chewing;
        stats.mouthState = s_chewing ? -0.2f : 0.4f;
        if (s_chewing) {
          GotchiHaptic::chew();
          // Miettes qui tombent de la bouche (max 6)
          if (s_crumbCount < 6) {
            float cx = MOUTH_X + (rand() % 40) - 20;
            float vx = ((rand() % 30) - 15) / 1000.0f;
            BehaviorObjects::spawn(ObjectShape::Circle, CRUMB_COLOR_DEFAULT, 4,
              cx, MOUTH_Y + 5.0f, vx, 0.02f, 0.0004f, 0, false, 1500);
            s_crumbCount++;
          }
        }
      }
    }

    // Blink de plaisir
    if (s_timer > 3500 && s_timer < 3550) {
      FaceEngine::blink();
      FaceEngine::setExpression(FaceExpression::Excited);
    }

    // Nourriture solide : arrêter après 5s total (~3s de mastication)
    if (!s_isBottle && s_timer > 5000) {
      BehaviorEngine::setAutoMode(true);
      BehaviorEngine::requestBehavior(&BEHAVIOR_IDLE);
    }
  }
}

static void onExit() {
  BehaviorObjects::destroyAll();
  BehaviorEngine::getStats().mouthState = 0.5f; // Sourire satisfait
  FaceEngine::setAutoMode(false);
}

static bool eatingOnTouch() {
  auto& stats = BehaviorEngine::getStats();
  if (stats.touchCount <= 2) {
    FaceEngine::setExpression(FaceExpression::Suspicious);
    stats.happiness -= 3;
    stats.clamp();
  } else {
    FaceEngine::setExpression(FaceExpression::Angry);
    stats.happiness -= 8;
    stats.irritability += 10;
    stats.clamp();
  }
  return true;
}

static bool eatingOnShake() {
  auto& stats = BehaviorEngine::getStats();
  FaceEngine::setExpression(FaceExpression::Angry);
  stats.happiness -= 5;
  stats.irritability += 15;
  stats.hunger -= 1;  // Recrache un petit bout
  stats.clamp();
  BehaviorEngine::requestBehavior(&BEHAVIOR_TANTRUM);
  return true;
}

void eatingSetFood(const char* food) {
  strncpy(s_foodName, food, sizeof(s_foodName) - 1);
  s_foodName[sizeof(s_foodName) - 1] = '\0';
}

const Behavior BEHAVIOR_EATING = {
  "eating", onEnter, onUpdate, onExit, eatingOnTouch, eatingOnShake,
  FaceExpression::Happy,
  3.0f, 5.0f,
  BF_USER_ACTION
};
