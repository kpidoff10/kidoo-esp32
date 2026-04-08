#include "overlay_sleep_zzz.h"
#include <cmath>
#include <cstring>
#include <esp_heap_caps.h>

using namespace OverlayCommon;

namespace {

float s_animT = 0.0f;

// Mini framebuffer couvrant toute la zone au-dessus des yeux
// Meme largeur que le FB principal pour pas voir de region separee
constexpr int16_t OVL_X = 20;
constexpr int16_t OVL_Y = 60;
constexpr int16_t OVL_W = 426;
constexpr int16_t OVL_H = 70;   // y=60 a y=130 (connecte au FB principal)
uint16_t* s_ovlBuf = nullptr;

inline void bufSetPixel(int16_t x, int16_t y, uint16_t color) {
  if (x >= 0 && x < OVL_W && y >= 0 && y < OVL_H)
    s_ovlBuf[y * OVL_W + x] = color;
}

inline void bufFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  for (int16_t j = y; j < y + h; j++)
    for (int16_t i = x; i < x + w; i++)
      bufSetPixel(i, j, color);
}

// Z carre (hauteur = largeur) dessine dans le buffer
void drawZ(int16_t cx, int16_t cy, int16_t size, uint16_t color) {
  int16_t half = size / 2;
  int16_t x0 = cx - half;
  int16_t y0 = cy - half;
  int16_t t = 3;

  // Barre haute
  bufFillRect(x0, y0, size, t, color);
  // Barre basse
  bufFillRect(x0, y0 + size - t, size, t, color);
  // Diagonale fine
  int16_t diagTop = y0 + t;
  int16_t diagBot = y0 + size - t - 1;
  int16_t diagH = diagBot - diagTop;
  if (diagH > 0) {
    for (int16_t dy = 0; dy <= diagH; dy++) {
      float u = (float)dy / (float)diagH;
      int16_t px = x0 + size - 1 - (int16_t)(u * (float)(size - 1));
      bufFillRect(px, diagTop + dy, t, 1, color);
    }
  }
}

} // namespace

namespace OverlaySleepZzz {

void update(float dtSec) {
  s_animT += dtSec;
}

void draw(Arduino_GFX* g, BBox& bbox) {
  if (!s_ovlBuf) {
    s_ovlBuf = (uint16_t*)heap_caps_malloc(OVL_W * OVL_H * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    if (!s_ovlBuf) return;
  }

  memset(s_ovlBuf, 0, OVL_W * OVL_H * sizeof(uint16_t));

  // 3 Z en cascade : grand -> moyen -> petit, flottent vers haut-droite
  // Coordonnees locales dans le buffer (0,0 = coin haut-gauche du buffer)
  constexpr float BASE_X = 230.0f;  // centre-droit du buffer
  constexpr float BASE_Y = 58.0f;   // bas du buffer
  constexpr float RISE   = 45.0f;   // montee verticale
  constexpr float DRIFT_X = 60.0f;  // derive vers la droite
  constexpr float CYCLE  = 16.0f;   // cycle lent pour bien espacer
  constexpr int NUM = 3;
  constexpr int16_t sizes[NUM] = {26, 19, 13};

  for (int i = 0; i < NUM; i++) {
    float phase = fmodf(s_animT + (float)i * (CYCLE / (float)NUM), CYCLE) / CYCLE;

    float alpha = 1.0f;
    if (phase < 0.1f) alpha = phase / 0.1f;
    else if (phase > 0.75f) alpha = (1.0f - phase) / 0.25f;
    if (alpha < 0.15f) continue;

    float y = BASE_Y - phase * RISE;
    float x = BASE_X + phase * DRIFT_X + sinf(s_animT * 1.2f + (float)i * 2.5f) * 4.0f;

    drawZ((int16_t)x, (int16_t)y, sizes[i], COL_CYAN);
  }

  g->draw16bitRGBBitmap(OVL_X, OVL_Y, s_ovlBuf, OVL_W, OVL_H);
  expand(bbox, OVL_X, OVL_Y, OVL_W, OVL_H);
}

void clear(Arduino_GFX* g) {
  if (s_ovlBuf && g) {
    memset(s_ovlBuf, 0, OVL_W * OVL_H * sizeof(uint16_t));
    g->draw16bitRGBBitmap(OVL_X, OVL_Y, s_ovlBuf, OVL_W, OVL_H);
  }
}

void reset() {
  s_animT = 0.0f;
}

} // namespace OverlaySleepZzz
