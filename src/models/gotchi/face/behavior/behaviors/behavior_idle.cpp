#include "../behavior_engine.h"
#include "../behavior_objects.h"
#include "../sprites/sprite_heart_24.h"
#include "../sprites/sprite_star_24.h"
#include "../sprites/sprite_sparkle_26.h"
#include "../sprites/sprite_note_22.h"
#include "../sprites/sprite_banana_22.h"
#include "../sprites/sprite_orange_22.h"
#include "../sprites/sprite_tennis_22.h"
#include "../sprites/sprite_apple_22.h"
#include "../sprites/sprite_strawberry_22.h"
#include "../sprites/sprite_bubbles_22.h"
#include "../sprites/sprite_airplane_36.h"
#include "../../face_engine.h"
#include "../../gotchi_haptic.h"
#include "../../../audio/gotchi_speaker_test.h"
#include "../../../audio/sounds/sound_sneeze.h"
#include <Arduino.h>
#include <cstdlib>
#include <cmath>

namespace {
uint32_t s_nextLook = 0;
uint32_t s_nextMicroExpr = 0;
uint32_t s_nextYawn = 0;
uint32_t s_idlePhase = 0;
bool s_justYawned = false;

// ----- Scenes idle (mini-animations) -----
// Une scene = sequence d'etapes timees, jouee en exclusivite (suspend les micro-expr normales).
enum class IdleScene : uint8_t {
  None,
  Daydream,    // regard derive doucement vers le haut + ennui + blinks lents
  LookAround,  // scrute autour de lui suspicieusement
  EyeRoll,     // yeux roulent en cercle (sceptique)
  DoubleTake,  // sursaute, regarde a droite puis gauche, double blink
  KnockKnock,  // tapote contre l'ecran "ouh ouh je suis la" + haptique
  // --- Scenes interactives ---
  Hum,         // chantonne, notes de musique qui montent + tete qui balance
  CatchStar,   // attrape une etoile filante des yeux et tape de joie
  ChaseFly,    // suit une "mouche" imaginaire en cercle (heart qui tourne)
  PeekABoo,    // jette un coup d'oeil par dessous (regard tout en bas)
  Wishful,     // regarde une etoile dans le ciel + soupir + coeur
  // --- Nouvelles scenes ---
  Stretch,     // s'etire longuement, yeux plisses puis grands ouverts
  Sneeze,      // eternuement : buildup + atchoo + surpris
  Dance,       // petite danse joyeuse avec sparkles
  Hiccup,      // serie de hoquets (micro-sursauts)
  NapAttempt,  // essaie de dormir, tete tombe, se reveille en sursaut
  // --- Scenes expressives (liees aux stats) ---
  Yawn,        // baillement complet, bouche grande ouverte, larme
  Whistle,     // siffler avec notes de musique
  Purr,        // ronronner de joie, haptic + coeurs
  StomachGrowl,// grogner du ventre, gene
  Reading,     // lire un livre (yeux gauche→droite, tag NFC)
  // --- Scenes joueur ---
  Juggle,      // jongler avec des balles
  Bubbles,     // souffler des bulles de savon
  Airplane,    // faire l'avion (yeux en cercle joyeux)
  Grimace,     // grimace rigolote
  PeekABoo2,   // coucou ameliore (se cache et surgit)
};

constexpr int IDLE_SCENE_COUNT = 25;

IdleScene s_scene = IdleScene::None;
uint32_t s_sceneTimer = 0;       // ms ecoulees dans la scene
uint8_t  s_sceneStep = 0;        // etape courante de la scene
uint32_t s_nextSceneIn = 0;      // ms avant prochaine scene candidate

constexpr uint32_t SCENE_MIN_DELAY = 25000;  // au moins 25s entre 2 scenes
constexpr uint32_t SCENE_RAND_DELAY = 35000; // + jusqu'a 35s aleatoire (total 25-60s)

void startScene(IdleScene scene) {
  // Reset propre de l'etat visuel meme si une scene precedente etait en cours.
  // Sinon transition d'expression et regard restent figes a mi-chemin.
  FaceEngine::setExpression(FaceExpression::Normal);
  FaceEngine::lookAt(0, 0);
  s_scene = scene;
  s_sceneTimer = 0;
  s_sceneStep = 0;
  Serial.printf("[IDLE] startScene state=%d\n", (int)scene);
}

void endScene() {
  s_scene = IdleScene::None;
  s_nextSceneIn = SCENE_MIN_DELAY + rand() % SCENE_RAND_DELAY;
  // Repositionner regard normal et expression de base
  FaceEngine::setExpression(FaceExpression::Normal);
  FaceEngine::lookAt(0, 0);
  Serial.println("[IDLE] endScene");
}

// Helpers pour passer a l'etape suivante a un timing absolu (ms depuis debut scene)
inline bool atStep(uint8_t step, uint32_t atMs) {
  return s_sceneStep == step && s_sceneTimer >= atMs;
}

void updateSceneDaydream(uint32_t /*dtMs*/) {
  // Regard qui derive vers le haut + Bored + blinks lents + bulles de pensee
  if (atStep(0, 0)) {
    FaceEngine::setExpression(FaceExpression::Bored);
    FaceEngine::lookAt(0.0f, -0.05f);
    s_sceneStep = 1;
  } else if (atStep(1, 800)) {
    FaceEngine::lookAt(-0.1f, -0.2f);
    FaceEngine::blink();
    // Petite bulle de pensee
    BehaviorObjects::spawn(ObjectShape::Circle, 0xFFFFFF, 4,
      280.0f, 180.0f, 0.005f, -0.02f, 0, 0, false, 2000);
    s_sceneStep = 2;
  } else if (atStep(2, 2000)) {
    FaceEngine::lookAt(0.05f, -0.35f);
    // Bulle moyenne
    BehaviorObjects::spawn(ObjectShape::Circle, 0xFFFFFF, 6,
      290.0f, 160.0f, 0.008f, -0.025f, 0, 0, false, 2000);
    s_sceneStep = 3;
  } else if (atStep(3, 3000)) {
    // Grande bulle + coeur ou etoile au sommet
    BehaviorObjects::spawn(ObjectShape::Circle, 0xFFFFFF, 9,
      300.0f, 140.0f, 0.01f, -0.03f, 0, 0, false, 2000);
    s_sceneStep = 4;
  } else if (atStep(4, 3800)) {
    // Sprite coeur au sommet de la rêverie
    BehaviorObjects::spawnSprite(SPRITE_HEART_24_ASSET, 0xFF6090,
      310.0f, 110.0f, 0.005f, -0.02f, 0, 0, false, 2000);
    FaceEngine::blink();
    FaceEngine::lookAt(-0.05f, -0.4f);
    s_sceneStep = 5;
  } else if (atStep(5, 5000)) {
    FaceEngine::blink();
    s_sceneStep = 6;
  } else if (atStep(6, 5800)) {
    endScene();
  }
}

void updateSceneLookAround(uint32_t /*dtMs*/) {
  // Scrute autour : Suspicious + regards rapides aux 4 coins.
  // Pas de bouche.
  if (atStep(0, 0)) {
    FaceEngine::setExpression(FaceExpression::Suspicious);
    FaceEngine::lookAt(-0.5f, -0.1f);
    s_sceneStep = 1;
  } else if (atStep(1, 500)) {
    FaceEngine::lookAt(0.5f, -0.1f);
    s_sceneStep = 2;
  } else if (atStep(2, 1000)) {
    FaceEngine::lookAt(0.5f, 0.2f);
    s_sceneStep = 3;
  } else if (atStep(3, 1500)) {
    FaceEngine::lookAt(-0.5f, 0.2f);
    s_sceneStep = 4;
  } else if (atStep(4, 2000)) {
    FaceEngine::lookAt(0, 0);
    FaceEngine::setExpression(FaceExpression::Skeptical);
    s_sceneStep = 5;
  } else if (atStep(5, 2700)) {
    endScene();
  }
}

void updateSceneEyeRoll(uint32_t /*dtMs*/) {
  // Yeux qui roulent en cercle, expression Skeptical/Annoyed.
  // Pas de bouche.
  if (atStep(0, 0)) {
    FaceEngine::setExpression(FaceExpression::Annoyed);
    s_sceneStep = 1;
  }
  // Cercle continu : 8 positions sur 1600ms
  if (s_scene == IdleScene::EyeRoll && s_sceneStep >= 1 && s_sceneStep <= 8) {
    uint32_t stepMs = 200 * (s_sceneStep - 1);
    if (s_sceneTimer >= stepMs + 100) {
      float angle = (float)(s_sceneStep - 1) * (6.2832f / 8.0f) - 1.5708f; // commence en haut
      float lx = cosf(angle) * 0.5f;
      float ly = sinf(angle) * 0.4f;
      FaceEngine::lookAt(lx, ly);
      s_sceneStep++;
    }
  }
  if (s_sceneStep == 9 && s_sceneTimer >= 1900) {
    FaceEngine::lookAt(0, 0);
    FaceEngine::blink();
    s_sceneStep = 10;
  } else if (s_sceneStep == 10 && s_sceneTimer >= 2300) {
    endScene();
  }
}

void updateSceneKnockKnock(uint32_t /*dtMs*/) {
  // "Ouh ouh je suis la" : 3 series de tapotement avec haptique synchro.
  // Pour chaque "tap" : Surprised (yeux ecarquilles, semble se rapprocher)
  // + lookAt vers le spectateur (0, 0.15) + bounce haptique.
  // Entre les taps : retour Normal. Pas de bouche.
  if (atStep(0, 0)) {
    FaceEngine::setExpression(FaceExpression::Surprised);
    FaceEngine::lookAt(0, 0.15f);
    GotchiHaptic::ballBounce();
    s_sceneStep = 1;
  } else if (atStep(1, 180)) {
    FaceEngine::setExpression(FaceExpression::Normal);
    FaceEngine::lookAt(0, 0);
    s_sceneStep = 2;
  } else if (atStep(2, 380)) {
    FaceEngine::setExpression(FaceExpression::Surprised);
    FaceEngine::lookAt(0, 0.15f);
    GotchiHaptic::ballBounce();
    s_sceneStep = 3;
  } else if (atStep(3, 560)) {
    FaceEngine::setExpression(FaceExpression::Normal);
    FaceEngine::lookAt(0, 0);
    s_sceneStep = 4;
  } else if (atStep(4, 900)) {
    // Petite pause puis 3e tap plus appuye
    FaceEngine::setExpression(FaceExpression::Amazed);
    FaceEngine::lookAt(0, 0.15f);
    GotchiHaptic::ballBounce();
    s_sceneStep = 5;
  } else if (atStep(5, 1080)) {
    FaceEngine::setExpression(FaceExpression::Excited);
    FaceEngine::lookAt(0, 0.1f);
    s_sceneStep = 6;
  } else if (atStep(6, 1500)) {
    FaceEngine::blink();
    s_sceneStep = 7;
  } else if (atStep(7, 1900)) {
    endScene();
  }
}

void updateSceneDoubleTake(uint32_t /*dtMs*/) {
  // Sursaut : voit qqch a droite, double blink, regarde a gauche, surpris.
  // Pas de bouche.
  if (atStep(0, 0)) {
    FaceEngine::lookAt(0.6f, 0);
    FaceEngine::setExpression(FaceExpression::Surprised);
    s_sceneStep = 1;
  } else if (atStep(1, 250)) {
    FaceEngine::blink();
    s_sceneStep = 2;
  } else if (atStep(2, 450)) {
    FaceEngine::blink();
    s_sceneStep = 3;
  } else if (atStep(3, 700)) {
    FaceEngine::lookAt(-0.6f, 0);
    FaceEngine::setExpression(FaceExpression::Amazed);
    s_sceneStep = 4;
  } else if (atStep(4, 1100)) {
    FaceEngine::lookAt(0, 0);
    FaceEngine::setExpression(FaceExpression::Confused);
    s_sceneStep = 5;
  } else if (atStep(5, 1800)) {
    endScene();
  }
}

// ----- Scene 6 : Hum (chantonne) -----
// Notes de musique qui montent + Happy + balancement leger des yeux gauche/droite.
void updateSceneHum(uint32_t /*dtMs*/) {
  if (atStep(0, 0)) {
    FaceEngine::setExpression(FaceExpression::Happy);
    FaceEngine::lookAt(-0.2f, -0.05f);
    BehaviorObjects::spawnSprite(SPRITE_NOTE_22_ASSET, 0,
      180.0f, 280.0f, 0.02f, -0.06f, 0, 0, false, 2500);
    s_sceneStep = 1;
  } else if (atStep(1, 600)) {
    FaceEngine::lookAt(0.2f, -0.05f);
    BehaviorObjects::spawnSprite(SPRITE_NOTE_22_ASSET, 0,
      280.0f, 280.0f, -0.02f, -0.05f, 0, 0, false, 2500);
    s_sceneStep = 2;
  } else if (atStep(2, 1200)) {
    FaceEngine::lookAt(-0.2f, -0.05f);
    BehaviorObjects::spawnSprite(SPRITE_NOTE_22_ASSET, 0,
      230.0f, 280.0f, 0.0f, -0.07f, 0, 0, false, 2500);
    s_sceneStep = 3;
  } else if (atStep(3, 1800)) {
    FaceEngine::lookAt(0.2f, -0.05f);
    BehaviorObjects::spawnSprite(SPRITE_NOTE_22_ASSET, 0,
      300.0f, 280.0f, -0.03f, -0.06f, 0, 0, false, 2500);
    s_sceneStep = 4;
  } else if (atStep(4, 2500)) {
    FaceEngine::lookAt(0, 0);
    FaceEngine::blink();
    s_sceneStep = 5;
  } else if (atStep(5, 3200)) {
    endScene();
  }
}

// ----- Scene 7 : CatchStar (etoile filante) -----
// Une etoile traverse l'ecran rapidement, les yeux la suivent, puis Amazed.
void updateSceneCatchStar(uint32_t /*dtMs*/) {
  if (atStep(0, 0)) {
    FaceEngine::setExpression(FaceExpression::Surprised);
    // Etoile filante : haut-gauche → bas-droite, vitesse moyenne, trackEyes=true
    BehaviorObjects::spawnSprite(SPRITE_STAR_24_ASSET, 0,
      80.0f, 160.0f, 0.18f, 0.06f, 0, 0, true, 2500);
    s_sceneStep = 1;
  } else if (atStep(1, 1800)) {
    // L'etoile est partie, reaction emerveillee
    FaceEngine::setExpression(FaceExpression::Amazed);
    FaceEngine::lookAt(0.4f, 0.3f);
    s_sceneStep = 2;
  } else if (atStep(2, 2400)) {
    FaceEngine::setExpression(FaceExpression::Excited);
    FaceEngine::lookAt(0, 0);
    s_sceneStep = 3;
  } else if (atStep(3, 3000)) {
    BehaviorObjects::destroyAll();
    endScene();
  }
}

// ----- Scene 8 : ChaseFly (suit une "mouche" imaginaire) -----
// Un sparkle qui tourne en cercle, les yeux le suivent comme une mouche.
void updateSceneChaseFly(uint32_t /*dtMs*/) {
  if (atStep(0, 0)) {
    FaceEngine::setExpression(FaceExpression::Confused);
    // Sparkle qui flotte au centre puis va etre teleporte par steps
    BehaviorObjects::spawnSprite(SPRITE_SPARKLE_26_ASSET, 0,
      233.0f, 200.0f, 0, 0, 0, 0, true, 4000);
    s_sceneStep = 1;
  }
  // Steps 1..6 : positions en cercle autour du visage (8 steps de 350ms chacun)
  if (s_sceneStep >= 1 && s_sceneStep <= 8) {
    uint32_t stepMs = 200 + 350 * (s_sceneStep - 1);
    if (s_sceneTimer >= stepMs) {
      // Re-spawn (l'ancien va expirer) au prochain point du cercle
      float angle = (float)(s_sceneStep - 1) * (6.2832f / 8.0f) - 1.5708f;
      float fx = 233.0f + cosf(angle) * 110.0f;
      float fy = 233.0f + sinf(angle) * 80.0f;
      BehaviorObjects::destroyAll();
      BehaviorObjects::spawnSprite(SPRITE_SPARKLE_26_ASSET, 0,
        fx, fy, 0, 0, 0, 0, true, 600);
      s_sceneStep++;
    }
  }
  if (s_sceneStep == 9 && s_sceneTimer >= 3200) {
    BehaviorObjects::destroyAll();
    FaceEngine::setExpression(FaceExpression::Annoyed);
    FaceEngine::lookAt(0, 0);
    s_sceneStep = 10;
  } else if (s_sceneStep == 10 && s_sceneTimer >= 3700) {
    endScene();
  }
}

// ----- Scene 9 : PeekABoo (regarde par dessous) -----
// Curieux : regarde tout en bas comme s'il cherchait sous l'ecran.
void updateScenePeekABoo(uint32_t /*dtMs*/) {
  if (atStep(0, 0)) {
    FaceEngine::setExpression(FaceExpression::Confused);
    FaceEngine::lookAt(0, 0.7f);
    s_sceneStep = 1;
  } else if (atStep(1, 700)) {
    FaceEngine::lookAt(-0.4f, 0.7f);
    s_sceneStep = 2;
  } else if (atStep(2, 1200)) {
    FaceEngine::lookAt(0.4f, 0.7f);
    s_sceneStep = 3;
  } else if (atStep(3, 1700)) {
    FaceEngine::setExpression(FaceExpression::Amazed);
    FaceEngine::lookAt(0, 0.5f);
    s_sceneStep = 4;
  } else if (atStep(4, 2200)) {
    FaceEngine::lookAt(0, 0);
    FaceEngine::setExpression(FaceExpression::Happy);
    s_sceneStep = 5;
  } else if (atStep(5, 2800)) {
    endScene();
  }
}

// ----- Scene 10 : Wishful (fait un voeu) -----
// Regarde une etoile en haut, soupir, et un coeur monte vers l'etoile.
void updateSceneWishful(uint32_t /*dtMs*/) {
  if (atStep(0, 0)) {
    FaceEngine::setExpression(FaceExpression::Pleading);
    FaceEngine::lookAt(0, -0.6f);
    // Etoile statique en haut a droite
    BehaviorObjects::spawnSprite(SPRITE_STAR_24_ASSET, 0,
      300.0f, 150.0f, 0, 0, 0, 0, false, 4500);
    s_sceneStep = 1;
  } else if (atStep(1, 800)) {
    FaceEngine::lookAt(0.3f, -0.6f);
    s_sceneStep = 2;
  } else if (atStep(2, 1500)) {
    // Coeur qui s'eleve vers l'etoile (la ou il regarde)
    BehaviorObjects::spawnSprite(SPRITE_HEART_24_ASSET, 0xFF6090,
      230.0f, 320.0f, 0.04f, -0.10f, 0, 0, false, 2500);
    FaceEngine::setExpression(FaceExpression::Happy);
    s_sceneStep = 3;
  } else if (atStep(3, 2800)) {
    FaceEngine::blink();
    s_sceneStep = 4;
  } else if (atStep(4, 3500)) {
    FaceEngine::lookAt(0, 0);
    FaceEngine::setExpression(FaceExpression::Normal);
    s_sceneStep = 5;
  } else if (atStep(5, 4200)) {
    BehaviorObjects::destroyAll();
    endScene();
  }
}

// ----- Scene 11 : Stretch (s'etire) -----
// Yeux plisses comme s'il s'etire, puis grands ouverts avec soulagement.
void updateSceneStretch(uint32_t /*dtMs*/) {
  if (atStep(0, 0)) {
    FaceEngine::setExpression(FaceExpression::Tired);
    FaceEngine::lookAt(0, -0.1f);
    s_sceneStep = 1;
  } else if (atStep(1, 400)) {
    // Yeux se ferment doucement pendant l'etirement
    FaceEngine::blink();
    FaceEngine::lookAt(0, -0.3f);
    s_sceneStep = 2;
  } else if (atStep(2, 1000)) {
    // Etirement max : expression forcee, regard haut
    FaceEngine::setExpression(FaceExpression::Annoyed);
    FaceEngine::lookAt(-0.1f, -0.4f);
    s_sceneStep = 3;
  } else if (atStep(3, 1800)) {
    // Relachement : yeux grands ouverts, soulagement
    FaceEngine::setExpression(FaceExpression::Amazed);
    FaceEngine::lookAt(0, 0);
    GotchiHaptic::joyTap();
    s_sceneStep = 4;
  } else if (atStep(4, 2400)) {
    FaceEngine::setExpression(FaceExpression::Happy);
    FaceEngine::blink();
    s_sceneStep = 5;
  } else if (atStep(5, 3000)) {
    endScene();
  }
}

// ----- Scene 12 : Sneeze (eternuement) -----
// Nez qui chatouille → buildup → ATCHOO ! → surpris + sparkles.
void updateSceneSneeze(uint32_t /*dtMs*/) {
  if (atStep(0, 0)) {
    // Debut : quelque chose chatouille
    FaceEngine::setExpression(FaceExpression::Confused);
    FaceEngine::lookAt(0, 0.1f);
    s_sceneStep = 1;
  } else if (atStep(1, 500)) {
    // Buildup 1 : yeux plisses
    FaceEngine::setExpression(FaceExpression::Annoyed);
    FaceEngine::lookAt(0, 0.05f);
    s_sceneStep = 2;
  } else if (atStep(2, 900)) {
    // Buildup 2 : tete recule (regard vers haut)
    FaceEngine::setExpression(FaceExpression::Vulnerable);
    FaceEngine::lookAt(0, -0.4f);
    s_sceneStep = 3;
  } else if (atStep(3, 1300)) {
    // ATCHOO ! Tete en avant + trauma + sparkles + son
    FaceEngine::setExpression(FaceExpression::Surprised);
    FaceEngine::lookAt(0, 0.5f);
    FaceEngine::trauma(0, 0.8f);
    GotchiHaptic::angerBurst();
    GotchiSpeakerTest::playSoundAsync(SNEEZE_PCM, SNEEZE_PCM_LEN);
    // Petites particules qui jaillissent
    BehaviorObjects::spawnSprite(SPRITE_SPARKLE_26_ASSET, 0,
      200.0f, 310.0f, -0.08f, -0.04f, 0, 0, false, 1500);
    BehaviorObjects::spawnSprite(SPRITE_SPARKLE_26_ASSET, 0,
      260.0f, 310.0f, 0.08f, -0.04f, 0, 0, false, 1500);
    BehaviorObjects::spawnSprite(SPRITE_SPARKLE_26_ASSET, 0,
      230.0f, 320.0f, 0.0f, -0.06f, 0, 0, false, 1500);
    s_sceneStep = 4;
  } else if (atStep(4, 1800)) {
    // Apres : confus, secoue la tete
    FaceEngine::setExpression(FaceExpression::Confused);
    FaceEngine::lookAt(0, 0);
    FaceEngine::blink();
    s_sceneStep = 5;
  } else if (atStep(5, 2500)) {
    FaceEngine::setExpression(FaceExpression::Embarrassed);
    s_sceneStep = 6;
  } else if (atStep(6, 3200)) {
    BehaviorObjects::destroyAll();
    endScene();
  }
}

// ----- Scene 13 : Dance (petite danse) -----
// Balance joyeusement de gauche a droite avec sparkles.
void updateSceneDance(uint32_t /*dtMs*/) {
  if (atStep(0, 0)) {
    FaceEngine::setExpression(FaceExpression::Excited);
    FaceEngine::lookAt(-0.3f, 0);
    s_sceneStep = 1;
  } else if (atStep(1, 350)) {
    FaceEngine::lookAt(0.3f, 0);
    BehaviorObjects::spawnSprite(SPRITE_SPARKLE_26_ASSET, 0,
      160.0f, 260.0f, 0.02f, -0.04f, 0, 0, false, 1500);
    s_sceneStep = 2;
  } else if (atStep(2, 700)) {
    FaceEngine::lookAt(-0.3f, -0.1f);
    GotchiHaptic::joyTap();
    s_sceneStep = 3;
  } else if (atStep(3, 1050)) {
    FaceEngine::lookAt(0.3f, -0.1f);
    BehaviorObjects::spawnSprite(SPRITE_SPARKLE_26_ASSET, 0,
      310.0f, 260.0f, -0.02f, -0.04f, 0, 0, false, 1500);
    s_sceneStep = 4;
  } else if (atStep(4, 1400)) {
    FaceEngine::lookAt(-0.2f, 0.1f);
    GotchiHaptic::joyTap();
    s_sceneStep = 5;
  } else if (atStep(5, 1750)) {
    FaceEngine::lookAt(0.2f, 0.1f);
    BehaviorObjects::spawnSprite(SPRITE_NOTE_22_ASSET, 0,
      230.0f, 280.0f, 0.0f, -0.06f, 0, 0, false, 2000);
    s_sceneStep = 6;
  } else if (atStep(6, 2100)) {
    FaceEngine::lookAt(-0.3f, 0);
    s_sceneStep = 7;
  } else if (atStep(7, 2450)) {
    FaceEngine::lookAt(0.3f, 0);
    GotchiHaptic::joyTap();
    BehaviorObjects::spawnSprite(SPRITE_SPARKLE_26_ASSET, 0,
      240.0f, 240.0f, 0.0f, -0.05f, 0, 0, false, 1500);
    s_sceneStep = 8;
  } else if (atStep(8, 2800)) {
    FaceEngine::setExpression(FaceExpression::Happy);
    FaceEngine::lookAt(0, 0);
    FaceEngine::blink();
    s_sceneStep = 9;
  } else if (atStep(9, 3500)) {
    BehaviorObjects::destroyAll();
    endScene();
  }
}

// ----- Scene 14 : Hiccup (hoquets) -----
// Serie de micro-sursauts involontaires, de plus en plus agaces.
void updateSceneHiccup(uint32_t /*dtMs*/) {
  if (atStep(0, 0)) {
    // Premier hoquet : surprise
    FaceEngine::setExpression(FaceExpression::Surprised);
    FaceEngine::trauma(0, 0.3f);
    GotchiHaptic::ballBounce();
    s_sceneStep = 1;
  } else if (atStep(1, 400)) {
    FaceEngine::setExpression(FaceExpression::Confused);
    FaceEngine::lookAt(0, 0);
    s_sceneStep = 2;
  } else if (atStep(2, 900)) {
    // Deuxieme hoquet
    FaceEngine::setExpression(FaceExpression::Surprised);
    FaceEngine::trauma(0, 0.3f);
    GotchiHaptic::ballBounce();
    FaceEngine::lookAt(0.1f, -0.1f);
    s_sceneStep = 3;
  } else if (atStep(3, 1300)) {
    FaceEngine::setExpression(FaceExpression::Annoyed);
    FaceEngine::lookAt(0, 0);
    s_sceneStep = 4;
  } else if (atStep(4, 1800)) {
    // Troisieme hoquet, plus fort
    FaceEngine::setExpression(FaceExpression::Surprised);
    FaceEngine::trauma(0, 0.5f);
    GotchiHaptic::ballBounce();
    FaceEngine::lookAt(-0.1f, 0.1f);
    s_sceneStep = 5;
  } else if (atStep(5, 2200)) {
    // Attend... c'est fini ?
    FaceEngine::setExpression(FaceExpression::Skeptical);
    FaceEngine::lookAt(0, 0);
    s_sceneStep = 6;
  } else if (atStep(6, 3000)) {
    // Soulagement
    FaceEngine::setExpression(FaceExpression::Happy);
    FaceEngine::blink();
    s_sceneStep = 7;
  } else if (atStep(7, 3600)) {
    endScene();
  }
}

// ----- Scene 15 : NapAttempt (essaie de dormir) -----
// Tete qui tombe doucement, puis se reveille en sursaut.
void updateSceneNapAttempt(uint32_t /*dtMs*/) {
  if (atStep(0, 0)) {
    FaceEngine::setExpression(FaceExpression::Tired);
    FaceEngine::lookAt(0, 0);
    s_sceneStep = 1;
  } else if (atStep(1, 600)) {
    // Yeux lourds, regard descend
    FaceEngine::setExpression(FaceExpression::Bored);
    FaceEngine::lookAt(0, 0.15f);
    FaceEngine::blink();
    s_sceneStep = 2;
  } else if (atStep(2, 1400)) {
    // Presque endormi, regard tout en bas
    FaceEngine::setExpression(FaceExpression::Tired);
    FaceEngine::lookAt(-0.05f, 0.4f);
    s_sceneStep = 3;
  } else if (atStep(3, 2200)) {
    // Blink tres lent (simule fermeture des yeux)
    FaceEngine::blink();
    FaceEngine::lookAt(-0.1f, 0.5f);
    s_sceneStep = 4;
  } else if (atStep(4, 3000)) {
    // SURSAUT ! Se reveille d'un coup
    FaceEngine::setExpression(FaceExpression::Surprised);
    FaceEngine::lookAt(0, -0.3f);
    FaceEngine::trauma(0, 0.4f);
    GotchiHaptic::ballBounce();
    s_sceneStep = 5;
  } else if (atStep(5, 3400)) {
    // Regarde autour, desoriente
    FaceEngine::setExpression(FaceExpression::Confused);
    FaceEngine::lookAt(-0.4f, 0);
    s_sceneStep = 6;
  } else if (atStep(6, 3800)) {
    FaceEngine::lookAt(0.4f, 0);
    s_sceneStep = 7;
  } else if (atStep(7, 4200)) {
    // Reprend ses esprits
    FaceEngine::setExpression(FaceExpression::Embarrassed);
    FaceEngine::lookAt(0, 0);
    FaceEngine::blink();
    s_sceneStep = 8;
  } else if (atStep(8, 4800)) {
    endScene();
  }
}

// ----- Scene 16 : Yawn (baillement complet) -----
// Bouche grande ouverte, yeux qui se ferment, petite larme.
void updateSceneYawn(uint32_t /*dtMs*/) {
  auto& stats = BehaviorEngine::getStats();
  if (atStep(0, 0)) {
    FaceEngine::setExpression(FaceExpression::Tired);
    FaceEngine::lookAt(0, 0);
    stats.mouthState = -0.2f;  // bouche commence a s'ouvrir
    s_sceneStep = 1;
  } else if (atStep(1, 500)) {
    FaceEngine::setExpression(FaceExpression::Bored);
    stats.mouthState = -0.5f;  // bouche s'ouvre plus
    s_sceneStep = 2;
  } else if (atStep(2, 1000)) {
    stats.mouthState = -0.9f;  // grande ouverture
    FaceEngine::blink();  // yeux se ferment
    // Petite larme/goutte a cote de l'oeil
    BehaviorObjects::spawn(ObjectShape::Drop, 0x6CF0FF, 4,
      310.0f, 200.0f, 0.01f, 0.03f, 0.0003f, 0, false, 1800);
    s_sceneStep = 3;
  } else if (atStep(3, 2200)) {
    stats.mouthState = -0.4f;  // bouche se referme
    FaceEngine::setExpression(FaceExpression::Tired);
    s_sceneStep = 4;
  } else if (atStep(4, 2800)) {
    stats.mouthState = 0.0f;
    FaceEngine::blink();
    FaceEngine::setExpression(FaceExpression::Normal);
    s_sceneStep = 5;
  } else if (atStep(5, 3500)) {
    endScene();
  }
}

// ----- Scene 17 : Whistle (siffler avec notes) -----
// Bouche en O, notes de musique qui sortent, tete qui balance.
void updateSceneWhistle(uint32_t /*dtMs*/) {
  auto& stats = BehaviorEngine::getStats();
  // Position de la bouche (doit matcher face_renderer.cpp)
  constexpr float MOUTH_X = 233.0f;
  constexpr float MOUTH_Y = 318.0f;
  if (atStep(0, 0)) {
    FaceEngine::setExpression(FaceExpression::Happy);
    FaceEngine::lookAt(-0.15f, -0.1f);
    stats.mouthState = -0.2f;  // bouche en O
    BehaviorObjects::spawnSprite(SPRITE_NOTE_22_ASSET, 0,
      MOUTH_X - 30.0f, MOUTH_Y - 20.0f, -0.02f, -0.06f, 0, 0, false, 2200);
    s_sceneStep = 1;
  } else if (atStep(1, 700)) {
    FaceEngine::lookAt(0.15f, -0.1f);
    BehaviorObjects::spawnSprite(SPRITE_NOTE_22_ASSET, 0,
      MOUTH_X + 20.0f, MOUTH_Y - 20.0f, 0.03f, -0.05f, 0, 0, false, 2200);
    s_sceneStep = 2;
  } else if (atStep(2, 1400)) {
    FaceEngine::lookAt(-0.15f, -0.1f);
    BehaviorObjects::spawnSprite(SPRITE_NOTE_22_ASSET, 0,
      MOUTH_X - 10.0f, MOUTH_Y - 20.0f, -0.01f, -0.07f, 0, 0, false, 2200);
    s_sceneStep = 3;
  } else if (atStep(3, 2100)) {
    FaceEngine::lookAt(0.15f, -0.1f);
    BehaviorObjects::spawnSprite(SPRITE_NOTE_22_ASSET, 0,
      MOUTH_X + 10.0f, MOUTH_Y - 20.0f, 0.02f, -0.06f, 0, 0, false, 2200);
    s_sceneStep = 4;
  } else if (atStep(4, 2800)) {
    FaceEngine::lookAt(0, 0);
    stats.mouthState = 0.3f;  // petit sourire satisfait
    FaceEngine::blink();
    s_sceneStep = 5;
  } else if (atStep(5, 3500)) {
    BehaviorObjects::spawnSprite(SPRITE_NOTE_22_ASSET, 0,
      MOUTH_X, MOUTH_Y - 20.0f, 0.0f, -0.08f, 0, 0, false, 1500);
    s_sceneStep = 6;
  } else if (atStep(6, 4200)) {
    endScene();
  }
}

// ----- Scene 18 : Purr (ronronner de joie) -----
// Haptic heartbeat + coeurs + expression beatique.
void updateScenePurr(uint32_t /*dtMs*/) {
  auto& stats = BehaviorEngine::getStats();
  if (atStep(0, 0)) {
    FaceEngine::setExpression(FaceExpression::Excited);
    FaceEngine::lookAt(0, 0);
    stats.mouthState = 0.7f;  // grand sourire
    GotchiHaptic::heartBeat();
    s_sceneStep = 1;
  } else if (atStep(1, 800)) {
    FaceEngine::setExpression(FaceExpression::Happy);
    BehaviorObjects::spawnSprite(SPRITE_HEART_24_ASSET, 0xFF6090,
      160.0f, 200.0f, -0.02f, -0.04f, 0, 0, false, 2000);
    s_sceneStep = 2;
  } else if (atStep(2, 1500)) {
    GotchiHaptic::heartBeat();
    BehaviorObjects::spawnSprite(SPRITE_HEART_24_ASSET, 0xFF6090,
      300.0f, 190.0f, 0.02f, -0.05f, 0, 0, false, 2000);
    s_sceneStep = 3;
  } else if (atStep(3, 2200)) {
    FaceEngine::blink();
    BehaviorObjects::spawnSprite(SPRITE_SPARKLE_26_ASSET, 0,
      230.0f, 160.0f, 0.0f, -0.03f, 0, 0, false, 1500);
    s_sceneStep = 4;
  } else if (atStep(4, 3000)) {
    GotchiHaptic::heartBeat();
    FaceEngine::setExpression(FaceExpression::Excited);
    FaceEngine::blink();
    s_sceneStep = 5;
  } else if (atStep(5, 4000)) {
    endScene();
  }
}

// ----- Scene 19 : StomachGrowl (grogner du ventre) -----
// Sursaut, regarde son ventre, expression genee, vibration.
void updateSceneStomachGrowl(uint32_t /*dtMs*/) {
  auto& stats = BehaviorEngine::getStats();
  if (atStep(0, 0)) {
    // Sursaut : le ventre gronde !
    FaceEngine::setExpression(FaceExpression::Surprised);
    FaceEngine::trauma(0, 0.3f);
    GotchiHaptic::ballBounce();
    s_sceneStep = 1;
  } else if (atStep(1, 500)) {
    // Regarde vers son ventre
    FaceEngine::setExpression(FaceExpression::Confused);
    FaceEngine::lookAt(0, 0.5f);
    s_sceneStep = 2;
  } else if (atStep(2, 1200)) {
    // Expression genee
    FaceEngine::setExpression(FaceExpression::Embarrassed);
    stats.mouthState = -0.2f;
    GotchiHaptic::ballBounce();
    FaceEngine::trauma(0, 0.2f);
    s_sceneStep = 3;
  } else if (atStep(3, 1800)) {
    // Regarde autour, gene
    FaceEngine::lookAt(-0.4f, 0);
    s_sceneStep = 4;
  } else if (atStep(4, 2200)) {
    FaceEngine::lookAt(0.4f, 0);
    s_sceneStep = 5;
  } else if (atStep(5, 2600)) {
    FaceEngine::lookAt(0, 0);
    FaceEngine::blink();
    stats.mouthState = 0.0f;
    s_sceneStep = 6;
  } else if (atStep(6, 3000)) {
    endScene();
  }
}

// ----- Scene 20 : Reading (lire un livre) -----
// Yeux qui bougent gauche→droite comme s'il lisait des lignes, parfois sourire.
void updateSceneReading(uint32_t /*dtMs*/) {
  auto& stats = BehaviorEngine::getStats();
  if (atStep(0, 0)) {
    FaceEngine::setExpression(FaceExpression::Normal);
    FaceEngine::lookAt(-0.4f, 0.1f);  // debut de ligne
    stats.mouthState = 0.0f;
    s_sceneStep = 1;
  } else if (atStep(1, 600)) {
    FaceEngine::lookAt(0.4f, 0.1f);  // fin de ligne 1
    s_sceneStep = 2;
  } else if (atStep(2, 1200)) {
    FaceEngine::lookAt(-0.4f, 0.15f);  // ligne 2
    s_sceneStep = 3;
  } else if (atStep(3, 1800)) {
    FaceEngine::lookAt(0.4f, 0.15f);
    s_sceneStep = 4;
  } else if (atStep(4, 2400)) {
    // Passage drole → sourire
    FaceEngine::lookAt(-0.4f, 0.2f);
    FaceEngine::setExpression(FaceExpression::Happy);
    stats.mouthState = 0.5f;
    s_sceneStep = 5;
  } else if (atStep(5, 3000)) {
    FaceEngine::lookAt(0.4f, 0.2f);
    s_sceneStep = 6;
  } else if (atStep(6, 3600)) {
    FaceEngine::lookAt(-0.4f, 0.25f);
    FaceEngine::setExpression(FaceExpression::Normal);
    stats.mouthState = 0.0f;
    s_sceneStep = 7;
  } else if (atStep(7, 4200)) {
    FaceEngine::lookAt(0.4f, 0.25f);
    FaceEngine::blink();
    s_sceneStep = 8;
  } else if (atStep(8, 4800)) {
    // Passage surprenant
    FaceEngine::setExpression(FaceExpression::Amazed);
    FaceEngine::lookAt(0, 0.15f);
    stats.mouthState = -0.3f;
    s_sceneStep = 9;
  } else if (atStep(9, 5400)) {
    FaceEngine::setExpression(FaceExpression::Happy);
    FaceEngine::lookAt(-0.4f, 0.3f);
    stats.mouthState = 0.4f;
    s_sceneStep = 10;
  } else if (atStep(10, 6000)) {
    FaceEngine::lookAt(0.4f, 0.3f);
    s_sceneStep = 11;
  } else if (atStep(11, 6600)) {
    FaceEngine::lookAt(0, 0);
    FaceEngine::blink();
    stats.mouthState = 0.3f;
    s_sceneStep = 12;
  } else if (atStep(12, 7200)) {
    endScene();
  }
}

// ----- Scene 21 : Juggle (jongler) -----
// Objets aleatoires (fruits, balles) qui montent/descendent. Rate le dernier.

const SpriteAsset* JUGGLE_ITEMS[] = {
  &SPRITE_APPLE_22_ASSET,
  &SPRITE_BANANA_22_ASSET,
  &SPRITE_ORANGE_22_ASSET,
  &SPRITE_STRAWBERRY_22_ASSET,
  &SPRITE_TENNIS_22_ASSET,
};
constexpr int JUGGLE_ITEM_COUNT = 5;

const SpriteAsset* s_juggleSet[3];  // 3 objets choisis au debut de la scene

inline const SpriteAsset& juggleRandom() {
  return *JUGGLE_ITEMS[rand() % JUGGLE_ITEM_COUNT];
}

void updateSceneJuggle(uint32_t /*dtMs*/) {
  if (atStep(0, 0)) {
    // Choisir 3 objets aleatoires pour cette session
    for (int i = 0; i < 3; i++) s_juggleSet[i] = JUGGLE_ITEMS[rand() % JUGGLE_ITEM_COUNT];
    FaceEngine::setExpression(FaceExpression::Happy);
    FaceEngine::lookAt(0, -0.3f);
    BehaviorObjects::spawnSprite(*s_juggleSet[0], 0,
      180.0f, 300.0f, 0.03f, -0.12f, 0.0006f, 0, false, 2500);
    s_sceneStep = 1;
  } else if (atStep(1, 600)) {
    FaceEngine::lookAt(-0.2f, -0.4f);
    BehaviorObjects::spawnSprite(*s_juggleSet[1], 0,
      230.0f, 300.0f, 0.0f, -0.13f, 0.0006f, 0, false, 2500);
    s_sceneStep = 2;
  } else if (atStep(2, 1200)) {
    FaceEngine::lookAt(0.2f, -0.4f);
    BehaviorObjects::spawnSprite(*s_juggleSet[2], 0,
      280.0f, 300.0f, -0.03f, -0.12f, 0.0006f, 0, false, 2500);
    s_sceneStep = 3;
  } else if (atStep(3, 1800)) {
    FaceEngine::lookAt(-0.3f, -0.3f);
    BehaviorObjects::spawnSprite(*s_juggleSet[0], 0,
      190.0f, 280.0f, 0.02f, -0.11f, 0.0006f, 0, false, 2000);
    s_sceneStep = 4;
  } else if (atStep(4, 2400)) {
    FaceEngine::lookAt(0, -0.4f);
    BehaviorObjects::spawnSprite(*s_juggleSet[1], 0,
      230.0f, 280.0f, -0.01f, -0.12f, 0.0006f, 0, false, 2000);
    s_sceneStep = 5;
  } else if (atStep(5, 3200)) {
    // Rate le dernier ! Il tombe
    FaceEngine::setExpression(FaceExpression::Surprised);
    FaceEngine::lookAt(0.3f, 0.5f);
    BehaviorObjects::spawnSprite(*s_juggleSet[2], 0,
      280.0f, 200.0f, 0.04f, 0.05f, 0.001f, 0, false, 1500);
    FaceEngine::trauma(0, 0.2f);
    s_sceneStep = 6;
  } else if (atStep(6, 4000)) {
    FaceEngine::setExpression(FaceExpression::Embarrassed);
    FaceEngine::lookAt(0, 0);
    FaceEngine::blink();
    s_sceneStep = 7;
  } else if (atStep(7, 4800)) {
    endScene();
  }
}

// ----- Scene 22 : Bubbles (bulles de savon) 🫧 -----
// Souffle des bulles emoji qui montent et derivent.
void updateSceneBubbles(uint32_t /*dtMs*/) {
  auto& stats = BehaviorEngine::getStats();
  if (atStep(0, 0)) {
    FaceEngine::setExpression(FaceExpression::Happy);
    stats.mouthState = -0.2f;  // bouche en O (souffle)
    FaceEngine::lookAt(0, -0.2f);
    s_sceneStep = 1;
  } else if (atStep(1, 400)) {
    float vx = ((rand() % 20) - 10) / 1000.0f;
    BehaviorObjects::spawnSprite(SPRITE_BUBBLES_22_ASSET, 0,
      233.0f, 290.0f, vx, -0.04f, 0, 0, false, 3000);
    s_sceneStep = 2;
  } else if (atStep(2, 1000)) {
    float vx = ((rand() % 20) - 10) / 1000.0f;
    BehaviorObjects::spawnSprite(SPRITE_BUBBLES_22_ASSET, 0,
      240.0f, 290.0f, vx, -0.035f, 0, 0, false, 3000);
    s_sceneStep = 3;
  } else if (atStep(3, 1600)) {
    float vx = ((rand() % 20) - 10) / 1000.0f;
    BehaviorObjects::spawnSprite(SPRITE_BUBBLES_22_ASSET, 0,
      225.0f, 290.0f, vx, -0.045f, 0, 0, false, 3000);
    FaceEngine::blink();
    s_sceneStep = 4;
  } else if (atStep(4, 2200)) {
    float vx = ((rand() % 20) - 10) / 1000.0f;
    BehaviorObjects::spawnSprite(SPRITE_BUBBLES_22_ASSET, 0,
      233.0f, 290.0f, vx, -0.05f, 0, 0, true, 2500);
    FaceEngine::setExpression(FaceExpression::Amazed);
    stats.mouthState = -0.3f;
    s_sceneStep = 5;
  } else if (atStep(5, 3200)) {
    FaceEngine::setExpression(FaceExpression::Excited);
    stats.mouthState = 0.5f;
    FaceEngine::blink();
    s_sceneStep = 6;
  } else if (atStep(6, 4000)) {
    endScene();
  }
}

// ----- Scene 23 : Airplane (avion passe au-dessus) ✈️ -----
// Un avion 36px traverse l'ecran, le gotchi le regarde passer.
void updateSceneAirplane(uint32_t /*dtMs*/) {
  if (atStep(0, 0)) {
    // Entend un bruit → regarde en l'air a gauche
    FaceEngine::setExpression(FaceExpression::Confused);
    FaceEngine::lookAt(-0.5f, -0.4f);
    s_sceneStep = 1;
  } else if (atStep(1, 600)) {
    // L'avion apparait a gauche et traverse vers la droite
    FaceEngine::setExpression(FaceExpression::Amazed);
    BehaviorObjects::spawnSprite(SPRITE_AIRPLANE_36_ASSET, 0,
      30.0f, 130.0f, 0.09f, -0.005f, 0, 0, true, 4000);
    s_sceneStep = 2;
  } else if (atStep(2, 1200)) {
    FaceEngine::lookAt(-0.2f, -0.4f);
    s_sceneStep = 3;
  } else if (atStep(3, 1800)) {
    FaceEngine::lookAt(0, -0.3f);
    FaceEngine::setExpression(FaceExpression::Excited);
    s_sceneStep = 4;
  } else if (atStep(4, 2400)) {
    FaceEngine::lookAt(0.3f, -0.3f);
    s_sceneStep = 5;
  } else if (atStep(5, 3000)) {
    FaceEngine::lookAt(0.5f, -0.2f);
    s_sceneStep = 6;
  } else if (atStep(6, 3600)) {
    // L'avion est parti
    FaceEngine::lookAt(0.5f, -0.1f);
    FaceEngine::setExpression(FaceExpression::Happy);
    s_sceneStep = 7;
  } else if (atStep(7, 4200)) {
    FaceEngine::lookAt(0, 0);
    FaceEngine::blink();
    s_sceneStep = 8;
  } else if (atStep(8, 4800)) {
    endScene();
  }
}

// ----- Scene 24 : Grimace -----
// Clins d'oeil alternés, bouche bizarre, content de sa betise.
void updateSceneGrimace(uint32_t /*dtMs*/) {
  auto& stats = BehaviorEngine::getStats();
  if (atStep(0, 0)) {
    FaceEngine::setExpression(FaceExpression::Skeptical);
    FaceEngine::blinkLeft();
    stats.mouthState = -0.6f;  // bouche tordue
    FaceEngine::lookAt(0.3f, 0.1f);
    s_sceneStep = 1;
  } else if (atStep(1, 700)) {
    FaceEngine::setExpression(FaceExpression::Annoyed);
    FaceEngine::blinkRight();
    stats.mouthState = 0.8f;
    FaceEngine::lookAt(-0.3f, -0.1f);
    s_sceneStep = 2;
  } else if (atStep(2, 1300)) {
    FaceEngine::blinkLeft();
    stats.mouthState = -0.5f;
    s_sceneStep = 3;
  } else if (atStep(3, 1600)) {
    FaceEngine::blinkRight();
    stats.mouthState = 0.7f;
    s_sceneStep = 4;
  } else if (atStep(4, 1900)) {
    FaceEngine::blinkLeft();
    stats.mouthState = -0.7f;
    s_sceneStep = 5;
  } else if (atStep(5, 2200)) {
    FaceEngine::blinkRight();
    stats.mouthState = 0.9f;
    s_sceneStep = 6;
  } else if (atStep(6, 2700)) {
    FaceEngine::setExpression(FaceExpression::Happy);
    FaceEngine::lookAt(0, 0);
    stats.mouthState = 0.6f;
    FaceEngine::blink();
    s_sceneStep = 7;
  } else if (atStep(7, 3400)) {
    endScene();
  }
}

// ----- Scene 25 : PeekABoo2 (coucou ameliore) -----
// Se cache, suspense, puis SURGIT avec sparkles de joie.
void updateScenePeekABoo2(uint32_t /*dtMs*/) {
  if (atStep(0, 0)) {
    FaceEngine::setExpression(FaceExpression::Normal);
    FaceEngine::lookAt(0, 0.3f);
    s_sceneStep = 1;
  } else if (atStep(1, 400)) {
    FaceEngine::lookAt(0, 0.6f);
    FaceEngine::setExpression(FaceExpression::Confused);
    s_sceneStep = 2;
  } else if (atStep(2, 800)) {
    FaceEngine::lookAt(0, 0.9f);
    s_sceneStep = 3;
  } else if (atStep(3, 1800)) {
    FaceEngine::lookAt(0.2f, 0.7f);
    FaceEngine::setExpression(FaceExpression::Suspicious);
    s_sceneStep = 4;
  } else if (atStep(4, 2200)) {
    FaceEngine::lookAt(0, 0.9f);
    s_sceneStep = 5;
  } else if (atStep(5, 2600)) {
    // SURGIT !
    FaceEngine::setExpression(FaceExpression::Excited);
    FaceEngine::lookAt(0, -0.4f);
    FaceEngine::trauma(0, 0.5f);
    GotchiHaptic::joyTap();
    // Sparkles de joie
    for (int i = 0; i < 3; i++) {
      float x = 150.0f + i * 80.0f + (rand() % 30);
      BehaviorObjects::spawnSprite(SPRITE_SPARKLE_26_ASSET, 0,
        x, 160.0f, ((rand() % 20) - 10) / 1000.0f, -0.04f,
        0, 0, false, 1500);
    }
    s_sceneStep = 6;
  } else if (atStep(6, 3100)) {
    FaceEngine::setExpression(FaceExpression::Happy);
    FaceEngine::lookAt(0, 0);
    FaceEngine::blink();
    s_sceneStep = 7;
  } else if (atStep(7, 3600)) {
    endScene();
  }
}

void updateScene(uint32_t dtMs) {
  if (s_scene == IdleScene::None) return;
  s_sceneTimer += dtMs;
  switch (s_scene) {
    case IdleScene::Daydream:    updateSceneDaydream(dtMs); break;
    case IdleScene::LookAround:  updateSceneLookAround(dtMs); break;
    case IdleScene::EyeRoll:     updateSceneEyeRoll(dtMs); break;
    case IdleScene::DoubleTake:  updateSceneDoubleTake(dtMs); break;
    case IdleScene::KnockKnock:  updateSceneKnockKnock(dtMs); break;
    case IdleScene::Hum:         updateSceneHum(dtMs); break;
    case IdleScene::CatchStar:   updateSceneCatchStar(dtMs); break;
    case IdleScene::ChaseFly:    updateSceneChaseFly(dtMs); break;
    case IdleScene::PeekABoo:    updateScenePeekABoo(dtMs); break;
    case IdleScene::Wishful:     updateSceneWishful(dtMs); break;
    case IdleScene::Stretch:     updateSceneStretch(dtMs); break;
    case IdleScene::Sneeze:      updateSceneSneeze(dtMs); break;
    case IdleScene::Dance:       updateSceneDance(dtMs); break;
    case IdleScene::Hiccup:      updateSceneHiccup(dtMs); break;
    case IdleScene::NapAttempt:  updateSceneNapAttempt(dtMs); break;
    case IdleScene::Yawn:        updateSceneYawn(dtMs); break;
    case IdleScene::Whistle:     updateSceneWhistle(dtMs); break;
    case IdleScene::Purr:        updateScenePurr(dtMs); break;
    case IdleScene::StomachGrowl:updateSceneStomachGrowl(dtMs); break;
    case IdleScene::Reading:     updateSceneReading(dtMs); break;
    case IdleScene::Juggle:      updateSceneJuggle(dtMs); break;
    case IdleScene::Bubbles:     updateSceneBubbles(dtMs); break;
    case IdleScene::Airplane:    updateSceneAirplane(dtMs); break;
    case IdleScene::Grimace:     updateSceneGrimace(dtMs); break;
    case IdleScene::PeekABoo2:   updateScenePeekABoo2(dtMs); break;
    default: break;
  }
}
}

static void onEnter() {
  FaceEngine::setExpression(FaceExpression::Normal);
  FaceEngine::setAutoMode(true);
  auto& stats = BehaviorEngine::getStats();
  stats.mouthState = 0.0f;
  s_nextLook = 1500 + rand() % 3000;
  s_nextMicroExpr = 6000 + rand() % 10000;
  s_nextYawn = 15000 + rand() % 20000;
  s_nextSceneIn = SCENE_MIN_DELAY + rand() % SCENE_RAND_DELAY;
  s_scene = IdleScene::None;
  s_sceneTimer = 0;
  s_sceneStep = 0;
  s_idlePhase = 0;
  s_justYawned = false;
}

static void onUpdate(uint32_t dtMs) {
  auto& stats = BehaviorEngine::getStats();
  s_idlePhase += dtMs;

  // Ennui monte doucement
  stats.addBoredom(0.5f, dtMs);

  // Bouche réactive aux stats
  if (stats.happiness > 60) stats.mouthState += (0.2f - stats.mouthState) * 0.01f;
  else stats.mouthState += (0.0f - stats.mouthState) * 0.01f;

  // --- Scenes idle (mini-animations exclusives) ---
  if (s_scene != IdleScene::None) {
    updateScene(dtMs);
    return;  // suspend les micro-mouvements normaux pendant une scene
  }

  // Decompte avant la prochaine scene
  if (s_nextSceneIn <= dtMs) {
    IdleScene chosen = IdleScene::None;
    const char* name = "?";

    // Scenes conditionnelles (liees aux stats) — priorite avant alea
    int chance = rand() % 100;
    if (stats.energy < 40 && chance < 30) {
      chosen = IdleScene::Yawn; name = "Yawn";
    } else if (stats.hunger < 40 && chance < 30) {
      chosen = IdleScene::StomachGrowl; name = "StomachGrowl";
    } else if (stats.happiness > 80 && chance < 20) {
      chosen = IdleScene::Purr; name = "Purr";
    } else if (stats.happiness > 60 && stats.boredom > 40 && chance < 20) {
      chosen = IdleScene::Whistle; name = "Whistle";
    }

    // Sinon selection aleatoire uniforme
    if (chosen == IdleScene::None) {
      int pick = rand() % IDLE_SCENE_COUNT;
      switch (pick) {
        case 0:  chosen = IdleScene::Daydream;    name = "Daydream";    break;
        case 1:  chosen = IdleScene::LookAround;  name = "LookAround";  break;
        case 2:  chosen = IdleScene::EyeRoll;     name = "EyeRoll";     break;
        case 3:  chosen = IdleScene::DoubleTake;  name = "DoubleTake";  break;
        case 4:  chosen = IdleScene::KnockKnock;  name = "KnockKnock";  break;
        case 5:  chosen = IdleScene::Hum;         name = "Hum";         break;
        case 6:  chosen = IdleScene::CatchStar;   name = "CatchStar";   break;
        case 7:  chosen = IdleScene::ChaseFly;    name = "ChaseFly";    break;
        case 8:  chosen = IdleScene::PeekABoo;    name = "PeekABoo";    break;
        case 9:  chosen = IdleScene::Wishful;     name = "Wishful";     break;
        case 10: chosen = IdleScene::Stretch;     name = "Stretch";     break;
        case 11: chosen = IdleScene::Sneeze;      name = "Sneeze";      break;
        case 12: chosen = IdleScene::Dance;       name = "Dance";       break;
        case 13: chosen = IdleScene::Hiccup;      name = "Hiccup";      break;
        case 14: chosen = IdleScene::NapAttempt;  name = "NapAttempt";  break;
        case 15: chosen = IdleScene::Yawn;        name = "Yawn";        break;
        case 16: chosen = IdleScene::Whistle;     name = "Whistle";     break;
        case 17: chosen = IdleScene::Purr;        name = "Purr";        break;
        case 18: chosen = IdleScene::StomachGrowl;name = "StomachGrowl";break;
        case 19: chosen = IdleScene::Reading;     name = "Reading";     break;
        case 20: chosen = IdleScene::Juggle;      name = "Juggle";      break;
        case 21: chosen = IdleScene::Bubbles;     name = "Bubbles";     break;
        case 22: chosen = IdleScene::Airplane;    name = "Airplane";    break;
        case 23: chosen = IdleScene::Grimace;     name = "Grimace";     break;
        case 24: chosen = IdleScene::PeekABoo2;   name = "PeekABoo2";  break;
      }
    }
    startScene(chosen);
    Serial.printf("[IDLE] Scene -> %s\n", name);
    return;
  } else {
    s_nextSceneIn -= dtMs;
  }

  // --- Micro-mouvements du regard ---
  if (s_nextLook <= dtMs) {
    float lx = ((float)(rand() % 300) - 150) / 500.0f;
    float ly = ((float)(rand() % 200) - 100) / 600.0f;
    FaceEngine::lookAt(lx, ly);
    s_nextLook = 2000 + rand() % 4000;
  } else {
    s_nextLook -= dtMs;
  }

  // --- Micro-expressions aléatoires ---
  if (s_nextMicroExpr <= dtMs) {
    int r = rand() % 8;
    switch (r) {
      case 0: FaceEngine::setExpression(FaceExpression::Confused); break;
      case 1: FaceEngine::setExpression(FaceExpression::Skeptical); break;
      case 2: FaceEngine::setExpression(FaceExpression::Bored); stats.mouthState = -0.1f; break;
      case 3: FaceEngine::setExpression(FaceExpression::Amazed); stats.mouthState = -0.3f; break;
      case 4: FaceEngine::setExpression(FaceExpression::Suspicious); break;
      case 5: FaceEngine::setExpression(FaceExpression::Happy); stats.mouthState = 0.4f; break;
      default: FaceEngine::setExpression(FaceExpression::Normal); stats.mouthState = 0.0f; break;
    }
    s_nextMicroExpr = 8000 + rand() % 15000;
  } else {
    s_nextMicroExpr -= dtMs;
  }

  // --- Bâillement si fatigué ---
  if (s_nextYawn <= dtMs) {
    if (stats.energy < 50 && !s_justYawned) {
      FaceEngine::setExpression(FaceExpression::Tired);
      stats.mouthState = -0.8f; // Grande bouche ouverte
      s_justYawned = true;
      s_nextYawn = 3000; // Durée du bâillement
    } else if (s_justYawned) {
      FaceEngine::setExpression(FaceExpression::Normal);
      stats.mouthState = 0.0f;
      FaceEngine::blink();
      s_justYawned = false;
      s_nextYawn = 20000 + rand() % 30000;
    } else {
      s_nextYawn = 15000 + rand() % 20000;
    }
  } else {
    s_nextYawn -= dtMs;
  }
}

static void onExit() {
  FaceEngine::setAutoMode(false);
}

static bool idleOnTouch() {
  auto& stats = BehaviorEngine::getStats();
  if (stats.happiness < 40) {
    FaceEngine::setExpression(FaceExpression::Pleading);
    FaceEngine::nod(FaceEngine::GestureSpeed::Slow);
    stats.happiness += 5;
    stats.clamp();
  } else {
    FaceEngine::nod(FaceEngine::GestureSpeed::Normal);
    stats.happiness += 8;
    stats.excitement += 15;
    stats.clamp();
    BehaviorEngine::requestBehavior(&BEHAVIOR_HAPPY);
  }
  return true;
}

static bool idleOnShake() {
  auto& stats = BehaviorEngine::getStats();
  // Secoue = surpris, mais ne lance PAS le jeu automatiquement
  FaceEngine::setExpression(FaceExpression::Surprised);
  stats.excitement += 15;
  stats.boredom -= 5;
  stats.clamp();
  return true;
}

const Behavior BEHAVIOR_IDLE = {
  "idle", onEnter, onUpdate, onExit, idleOnTouch, idleOnShake,
  FaceExpression::Normal,
  30.0f,  // min 30s — le temps de voir au moins une scene idle (deconnectees toutes les 12-30s)
  90.0f,  // max 90s
  BF_NONE
};

// API publique : forcer le declenchement d'une scene idle (pour tests serial).
// num: 1..10 — voir IdleScene enum
bool idleTriggerScene(int num) {
  if (num < 1 || num > IDLE_SCENE_COUNT) return false;
  IdleScene scene = IdleScene::None;
  const char* name = "?";
  switch (num) {
    case 1:  scene = IdleScene::Daydream;   name = "Daydream";   break;
    case 2:  scene = IdleScene::LookAround; name = "LookAround"; break;
    case 3:  scene = IdleScene::EyeRoll;    name = "EyeRoll";    break;
    case 4:  scene = IdleScene::DoubleTake; name = "DoubleTake"; break;
    case 5:  scene = IdleScene::KnockKnock; name = "KnockKnock"; break;
    case 6:  scene = IdleScene::Hum;        name = "Hum";        break;
    case 7:  scene = IdleScene::CatchStar;  name = "CatchStar";  break;
    case 8:  scene = IdleScene::ChaseFly;   name = "ChaseFly";   break;
    case 9:  scene = IdleScene::PeekABoo;   name = "PeekABoo";   break;
    case 10: scene = IdleScene::Wishful;    name = "Wishful";    break;
    case 11: scene = IdleScene::Stretch;    name = "Stretch";    break;
    case 12: scene = IdleScene::Sneeze;     name = "Sneeze";     break;
    case 13: scene = IdleScene::Dance;      name = "Dance";      break;
    case 14: scene = IdleScene::Hiccup;     name = "Hiccup";     break;
    case 15: scene = IdleScene::NapAttempt;  name = "NapAttempt";  break;
    case 16: scene = IdleScene::Yawn;        name = "Yawn";        break;
    case 17: scene = IdleScene::Whistle;     name = "Whistle";     break;
    case 18: scene = IdleScene::Purr;        name = "Purr";        break;
    case 19: scene = IdleScene::StomachGrowl;name = "StomachGrowl";break;
    case 20: scene = IdleScene::Reading;     name = "Reading";     break;
    case 21: scene = IdleScene::Juggle;      name = "Juggle";      break;
    case 22: scene = IdleScene::Bubbles;     name = "Bubbles";     break;
    case 23: scene = IdleScene::Airplane;    name = "Airplane";    break;
    case 24: scene = IdleScene::Grimace;     name = "Grimace";     break;
    case 25: scene = IdleScene::PeekABoo2;   name = "PeekABoo2";  break;
  }
  startScene(scene);
  Serial.printf("[IDLE] Scene forcee -> %d %s\n", num, name);
  return true;
}
