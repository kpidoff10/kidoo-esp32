#include "poop_manager.h"
#include "behavior_objects.h"
#include "behavior_engine.h"
#include "sprites/sprite_poop_emoji_44.h"
#include <cstdlib>

namespace {

constexpr int MAX_POOPS = 4;
constexpr int16_t HIT_RADIUS = 30;              // Rayon de détection touch (px)
constexpr uint32_t SPAWN_MIN_MS = 90UL * 60000;  // Min 1h30 entre deux cacas
constexpr uint32_t SPAWN_RANGE_MS = 90UL * 60000; // + 0-1h30 aléatoire (total 1h30-3h)
constexpr uint32_t DEGRADE_INTERVAL = 5000; // Dégrade stats toutes les 5s par caca
constexpr float HYGIENE_PENALTY = 2.0f;    // Par caca, toutes les 5s
constexpr float HEALTH_PENALTY = 1.0f;     // Si 3+ cacas, toutes les 5s

struct Poop {
  int objId = -1;
  float x = 0, y = 0;
  bool alive = false;
};

Poop s_poops[MAX_POOPS];
uint32_t s_spawnTimer = 0;
uint32_t s_nextSpawnIn = 15000;  // Premier caca après 15s
uint32_t s_degradeTimer = 0;

// Positions possibles pour les cacas (autour du visage, dans la zone visible)
// Écran rond 466x466 — le framebuffer couvre y=130-400, garder une marge
void randomPoopPosition(float& x, float& y) {
  int zone = rand() % 5;
  switch (zone) {
    case 0: x = 120 + rand() % 40; y = 300 + rand() % 40; break;  // bas-gauche
    case 1: x = 310 + rand() % 40; y = 300 + rand() % 40; break;  // bas-droite
    case 2: x = 190 + rand() % 80; y = 350 + rand() % 20; break;  // bas-centre
    case 3: x = 90 + rand() % 40;  y = 240 + rand() % 50; break;  // gauche
    case 4: x = 340 + rand() % 40; y = 240 + rand() % 50; break;  // droite
  }
}

int poopCount() {
  int n = 0;
  for (int i = 0; i < MAX_POOPS; i++) {
    if (s_poops[i].alive) n++;
  }
  return n;
}

} // namespace

namespace PoopManager {

void init() {
  for (int i = 0; i < MAX_POOPS; i++) {
    s_poops[i].alive = false;
    s_poops[i].objId = -1;
  }
  s_spawnTimer = 0;
  s_nextSpawnIn = SPAWN_MIN_MS + rand() % SPAWN_RANGE_MS;
  s_degradeTimer = 0;
}

void update(uint32_t dtMs) {
  // --- Spawn aléatoire ---
  s_spawnTimer += dtMs;
  if (s_spawnTimer >= s_nextSpawnIn) {
    s_spawnTimer = 0;
    s_nextSpawnIn = SPAWN_MIN_MS + rand() % SPAWN_RANGE_MS;

    // Trouver un slot libre
    for (int i = 0; i < MAX_POOPS; i++) {
      if (!s_poops[i].alive) {
        float px, py;
        randomPoopPosition(px, py);
        int id = BehaviorObjects::spawnSprite(
          SPRITE_POOP_EMOJI_44_ASSET, 0,
          px, py, 0, 0, 0, 0, false, 0);  // lifetime 0 = infini
        if (id >= 0) {
          s_poops[i].objId = id;
          s_poops[i].x = px;
          s_poops[i].y = py;
          s_poops[i].alive = true;
        }
        break;
      }
    }
  }

  // --- Dégradation des stats ---
  int count = poopCount();
  if (count > 0) {
    s_degradeTimer += dtMs;
    if (s_degradeTimer >= DEGRADE_INTERVAL) {
      s_degradeTimer = 0;
      auto& stats = BehaviorEngine::getStats();
      // Chaque caca dégrade l'hygiène
      stats.hygiene -= HYGIENE_PENALTY * count;
      // 3+ cacas = dégradation santé aussi
      if (count >= 3) {
        stats.health -= HEALTH_PENALTY;
      }
      stats.clamp();
    }
  } else {
    s_degradeTimer = 0;
  }
}

bool onTouchAt(int16_t x, int16_t y) {
  for (int i = 0; i < MAX_POOPS; i++) {
    if (!s_poops[i].alive) continue;
    float dx = x - s_poops[i].x;
    float dy = y - s_poops[i].y;
    if (dx * dx + dy * dy < HIT_RADIUS * HIT_RADIUS) {
      // Touché ! Nettoyer ce caca
      BehaviorObjects::destroy(s_poops[i].objId);
      s_poops[i].alive = false;
      s_poops[i].objId = -1;
      // Petit bonus hygiène
      auto& stats = BehaviorEngine::getStats();
      stats.hygiene += 5;
      stats.happiness += 2;
      stats.clamp();
      return true;
    }
  }
  return false;
}

void cleanAll() {
  for (int i = 0; i < MAX_POOPS; i++) {
    if (s_poops[i].alive) {
      BehaviorObjects::destroy(s_poops[i].objId);
      s_poops[i].alive = false;
      s_poops[i].objId = -1;
    }
  }
}

void spawnOne() {
  for (int i = 0; i < MAX_POOPS; i++) {
    if (!s_poops[i].alive) {
      float px, py;
      randomPoopPosition(px, py);
      int id = BehaviorObjects::spawnSprite(
        SPRITE_POOP_EMOJI_44_ASSET, 0,
        px, py, 0, 0, 0, 0, false, 0);
      if (id >= 0) {
        s_poops[i].objId = id;
        s_poops[i].x = px;
        s_poops[i].y = py;
        s_poops[i].alive = true;
      }
      return;
    }
  }
}

int getCount() {
  return poopCount();
}

} // namespace PoopManager
