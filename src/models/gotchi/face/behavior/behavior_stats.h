#ifndef BEHAVIOR_STATS_H
#define BEHAVIOR_STATS_H

#include <cstdint>
#include <Arduino.h>

struct BehaviorStats {
  // === Besoins vitaux (0 = critique, 100 = parfait) ===
  float hunger    = 70.0f;   // 0=affamé, 100=rassasié
  float energy    = 80.0f;   // 0=épuisé, 100=pleine forme
  float happiness = 60.0f;   // 0=déprimé, 100=euphorique
  float health    = 90.0f;   // 0=très malade, 100=parfaite santé
  float hygiene   = 70.0f;   // 0=très sale, 100=propre

  // === États temporaires ===
  float excitement = 0.0f;   // 0=calme, 100=surexcité (décroît vite)
  float boredom    = 20.0f;  // 0=diverti, 100=ennui total

  // === Méta ===
  uint32_t ageMinutes = 0;
  uint32_t ageTimer   = 0;   // Compteur interne pour le calcul d'âge
  uint32_t lastFedAt  = 0;
  uint32_t lastPlayAt = 0;

  // === Bouche ===
  // -1.0 = grande ouverte (faim/surprise), 0 = fermée, 1.0 = sourire
  float mouthState = 0.0f;

  void clamp() {
    auto c = [](float& v) { if (v < 0) v = 0; if (v > 100) v = 100; };
    c(hunger); c(energy); c(happiness); c(health); c(hygiene);
    c(excitement); c(boredom);
    if (mouthState < -1.0f) mouthState = -1.0f;
    if (mouthState > 1.0f)  mouthState = 1.0f;
  }

  // Évolution naturelle (appelé chaque frame)
  void decay(uint32_t dtMs) {
    float sec = dtMs / 1000.0f;
    float min = sec / 60.0f;

    // --- Decay de base ---
    hunger     -= 2.0f  * min;   // Faim monte (stat baisse)
    energy     -= 0.8f  * min;
    happiness  -= 0.3f  * min;
    health     -= 0.1f  * min;   // Très lent naturellement
    hygiene    -= 1.5f  * min;
    boredom    += 1.5f  * min;
    excitement -= 8.0f  * min;   // Décroît vite

    // --- Interactions entre stats (cercles vicieux) ---
    if (hunger < 10)  health    -= 0.5f * min;  // La faim rend malade
    if (hygiene < 15) health    -= 0.3f * min;  // La saleté rend malade
    if (health < 30)  happiness -= 1.0f * min;  // Être malade rend triste
    if (energy < 10)  happiness -= 0.5f * min;  // La fatigue rend triste
    if (boredom > 80) happiness -= 0.3f * min;  // L'ennui rend triste

    // --- Âge ---
    ageTimer += dtMs;
    if (ageTimer >= 60000) {
      ageTimer -= 60000;
      ageMinutes++;
    }

    // --- Bouche réactive aux stats ---
    float targetMouth = 0.0f;
    if (hunger < 15)        targetMouth = -0.8f;  // Bouche ouverte (faim)
    else if (happiness > 70) targetMouth = 0.7f;   // Sourire
    else if (health < 25)    targetMouth = -0.4f;  // Grimace
    else if (happiness < 25) targetMouth = -0.3f;  // Moue
    mouthState += (targetMouth - mouthState) * 0.02f * sec; // Smooth transition

    clamp();
  }

  // --- Modifiers (utilisés par les behaviors) ---
  void addHunger(float v, uint32_t dtMs)     { hunger     += v * dtMs / 1000.0f; clamp(); }
  void addEnergy(float v, uint32_t dtMs)     { energy     += v * dtMs / 1000.0f; clamp(); }
  void addHappiness(float v, uint32_t dtMs)  { happiness  += v * dtMs / 1000.0f; clamp(); }
  void addHealth(float v, uint32_t dtMs)     { health     += v * dtMs / 1000.0f; clamp(); }
  void addHygiene(float v, uint32_t dtMs)    { hygiene    += v * dtMs / 1000.0f; clamp(); }
  void addBoredom(float v, uint32_t dtMs)    { boredom    += v * dtMs / 1000.0f; clamp(); }
  void addExcitement(float v, uint32_t dtMs) { excitement += v * dtMs / 1000.0f; clamp(); }

  // --- Feed (applique les effets de nourriture) ---
  void feed(const char* food) {
    lastFedAt = millis();
    if (strcmp(food, "bottle") == 0)     { hunger += 25; happiness += 5;  }
    else if (strcmp(food, "cake") == 0)  { hunger += 10; happiness += 20; health += 5; }
    else if (strcmp(food, "apple") == 0) { hunger += 15; health += 10; }
    else if (strcmp(food, "candy") == 0) { hunger += 8;  happiness += 15; health -= 3; }
    else                                 { hunger += 15; happiness += 5; } // Nourriture générique
    mouthState = 0.5f; // Sourire après manger
    clamp();
  }

  // --- Soigner ---
  void heal() { health += 30; happiness += 10; clamp(); }

  // --- Nettoyer ---
  void clean() { hygiene += 40; happiness += 5; clamp(); }

  // --- Log ---
  void printStats(const char* behavior) {
    Serial.printf("[STATS] hunger=%.0f energy=%.0f happy=%.0f health=%.0f hygiene=%.0f "
                  "excite=%.0f bored=%.0f age=%lum | %s\n",
      hunger, energy, happiness, health, hygiene,
      excitement, boredom, ageMinutes, behavior);
  }
};

#endif
