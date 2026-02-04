#ifndef EMOTION_MANAGER_H
#define EMOTION_MANAGER_H

#include <Arduino.h>

/**
 * Gestionnaire d'émotions pour le Gotchi (Tamagotchi)
 *
 * Affiche un visage type Tamagotchi sur le LCD avec une émotion courante.
 * Chaque émotion a sa propre fonction de dessin (yeux + bouche).
 */

enum class Emotion : uint8_t {
  Happy = 0,   // Sourire
  Sad,         // Triste
  Hungry,      // Faim
  Sleepy,      // Endormi
  Sick,        // Malade
  Angry,       // En colère
  Neutral,     // Neutre
  Count
};

class EmotionManager {
public:
  /** Initialiser le gestionnaire (optionnel, garde l'émotion courante) */
  static bool init();

  /** Définir l'émotion courante et redessiner le visage */
  static void setEmotion(Emotion e);

  /** Obtenir l'émotion courante */
  static Emotion getEmotion();

  /** Dessiner le visage complet pour l'émotion courante (fond noir + visage) */
  static void drawFace();

  /** Dessiner le visage pour une émotion donnée (sans changer l'état) */
  static void drawFace(Emotion e);

  // --- Fonctions par émotion (dessin yeux + bouche) ---
  static void drawHappy();    // Sourire
  static void drawSad();      // Triste
  static void drawHungry();   // Faim
  static void drawSleepy();   // Endormi (yeux fermés / demi)
  static void drawSick();    // Malade
  static void drawAngry();   // En colère
  static void drawNeutral(); // Neutre

private:
  static Emotion _current;
  static void drawEyes(int16_t cx, int16_t cy, int16_t eyeRadius, uint16_t color);
  static void drawSmile(int16_t cx, int16_t mouthY, int16_t radius, uint16_t color);
  static void drawFrown(int16_t cx, int16_t mouthY, int16_t radius, uint16_t color);
  static void drawLineMouth(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
};

#endif // EMOTION_MANAGER_H
