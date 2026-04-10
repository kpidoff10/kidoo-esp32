#include "overlay_status.h"
#include "../behavior/behavior_engine.h"
#include <cmath>
#include <cstring>
#include <esp_heap_caps.h>

using namespace OverlayCommon;

namespace {

// Mini framebuffer pour la barre de status (au-dessus des yeux, zone visible du cercle)
constexpr int16_t BAR_W = 200;
constexpr int16_t BAR_H = 28;
constexpr int16_t BAR_X = (GOTCHI_LCD_WIDTH - BAR_W) / 2;  // Centre horizontal
constexpr int16_t BAR_Y = 50;  // Assez bas pour etre visible sur ecran rond
uint16_t* s_barBuf = nullptr;
bool s_wasDrawn = false;
float s_blinkTimer = 0;

// Couleurs
constexpr uint16_t COL_OK      = 0x0000;  // Pas affiche
constexpr uint16_t COL_WARN    = 0xFD20;  // Orange ~FF6600
constexpr uint16_t COL_CRIT    = 0xF800;  // Rouge
constexpr uint16_t COL_GREEN   = 0x07E0;  // Vert

inline void barPx(int16_t x, int16_t y, uint16_t color) {
  if (x >= 0 && x < BAR_W && y >= 0 && y < BAR_H)
    s_barBuf[y * BAR_W + x] = color;
}

inline void barFillCircle(int16_t cx, int16_t cy, int16_t r, uint16_t color) {
  for (int16_t dy = -r; dy <= r; dy++) {
    int16_t dx = (int16_t)sqrtf((float)(r * r - dy * dy));
    for (int16_t x = cx - dx; x <= cx + dx; x++)
      barPx(x, cy + dy, color);
  }
}

inline void barFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  for (int16_t j = y; j < y + h; j++)
    for (int16_t i = x; i < x + w; i++)
      barPx(i, j, color);
}

// Lune (energy) : croissant
void drawMoon(int16_t cx, int16_t cy, int16_t r, uint16_t color) {
  barFillCircle(cx, cy, r, color);
  barFillCircle(cx + r / 2, cy - r / 4, r, 0x0000);  // Decoupe
}

// Coeur (health)
void drawHeart(int16_t cx, int16_t cy, int16_t size, uint16_t color) {
  int16_t r = size / 3;
  barFillCircle(cx - r, cy - r / 2, r, color);
  barFillCircle(cx + r, cy - r / 2, r, color);
  // Triangle bas
  for (int16_t dy = 0; dy < size; dy++) {
    float u = (float)dy / (float)size;
    int16_t dx = (int16_t)((float)(size) * (1.0f - u));
    for (int16_t x = cx - dx; x <= cx + dx; x++)
      barPx(x, cy + dy, color);
  }
}

// Goutte (hygiene)
void drawDrop(int16_t cx, int16_t cy, int16_t size, uint16_t color) {
  int16_t r = size / 3;
  barFillCircle(cx, cy + r, r, color);
  for (int16_t dy = 0; dy < size; dy++) {
    float u = (float)dy / (float)size;
    int16_t dx = (int16_t)((float)r * u);
    for (int16_t x = cx - dx; x <= cx + dx; x++)
      barPx(x, cy - size / 2 + dy, color);
  }
}

// Os (hunger)
void drawBone(int16_t cx, int16_t cy, int16_t size, uint16_t color) {
  int16_t w = size;
  int16_t t = size / 4;
  if (t < 2) t = 2;
  barFillRect(cx - w / 2, cy - t / 2, w, t, color);
  int16_t r = t;
  barFillCircle(cx - w / 2, cy - t, r, color);
  barFillCircle(cx - w / 2, cy + t, r, color);
  barFillCircle(cx + w / 2, cy - t, r, color);
  barFillCircle(cx + w / 2, cy + t, r, color);
}

// Eclair (excitement/boredom)
void drawBolt(int16_t cx, int16_t cy, int16_t size, uint16_t color) {
  int16_t h = size;
  int16_t w = size / 2;
  // Haut
  for (int16_t dy = 0; dy < h / 2; dy++) {
    float u = (float)dy / (float)(h / 2);
    int16_t px = cx + w / 2 - (int16_t)(u * w);
    barFillRect(px, cy - h / 2 + dy, 3, 1, color);
  }
  // Bas
  for (int16_t dy = 0; dy < h / 2; dy++) {
    float u = (float)dy / (float)(h / 2);
    int16_t px = cx - w / 2 + (int16_t)(u * w);
    barFillRect(px, cy + dy, 3, 1, color);
  }
}

struct StatusIcon {
  bool show;
  uint16_t color;
};

StatusIcon getEnergyIcon(float energy) {
  if (energy < 15) return { true, COL_CRIT };
  if (energy < 35) return { true, COL_WARN };
  return { false, 0 };
}

StatusIcon getHealthIcon(float health) {
  if (health < 20) return { true, COL_CRIT };
  if (health < 40) return { true, COL_WARN };
  return { false, 0 };
}

StatusIcon getHungerIcon(float hunger) {
  if (hunger < 15) return { true, COL_CRIT };
  if (hunger < 30) return { true, COL_WARN };
  return { false, 0 };
}

StatusIcon getHygieneIcon(float hygiene) {
  if (hygiene < 15) return { true, COL_CRIT };
  if (hygiene < 30) return { true, COL_WARN };
  return { false, 0 };
}

} // namespace

namespace OverlayStatus {

void update(float dtSec) {
  s_blinkTimer += dtSec;
}

void draw(Arduino_GFX* g, BBox& bbox) {
  if (!g) return;

  auto& stats = BehaviorEngine::getStats();

  // Quels icones afficher ?
  StatusIcon energy  = getEnergyIcon(stats.energy);
  StatusIcon health  = getHealthIcon(stats.health);
  StatusIcon hunger  = getHungerIcon(stats.hunger);
  StatusIcon hygiene = getHygieneIcon(stats.hygiene);

  bool anyToShow = energy.show || health.show || hunger.show || hygiene.show;

  if (!anyToShow && !s_wasDrawn) return;

  // Allouer le buffer
  if (!s_barBuf) {
    s_barBuf = (uint16_t*)heap_caps_malloc(BAR_W * BAR_H * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    if (!s_barBuf) return;
  }

  // Clear
  memset(s_barBuf, 0, BAR_W * BAR_H * sizeof(uint16_t));

  if (anyToShow) {
    bool blinkOn = ((int)(s_blinkTimer * 2.0f) % 2) == 0;
    constexpr int16_t iconSize = 10;
    constexpr int16_t iconSpacing = 40;
    constexpr int16_t iconY = BAR_H / 2;

    // Compter combien d'icones a afficher pour centrer
    int count = 0;
    if (energy.show) count++;
    if (health.show) count++;
    if (hunger.show) count++;
    if (hygiene.show) count++;

    // Centrer horizontalement
    int16_t totalW = (int16_t)(count * iconSpacing - (iconSpacing - iconSize * 2));
    int16_t iconX = (BAR_W - totalW) / 2 + iconSize;

    if (energy.show) {
      bool crit = (energy.color == COL_CRIT);
      if (!crit || blinkOn) drawMoon(iconX, iconY, iconSize, energy.color);
      iconX += iconSpacing;
    }
    if (health.show) {
      bool crit = (health.color == COL_CRIT);
      if (!crit || blinkOn) drawHeart(iconX, iconY, iconSize, health.color);
      iconX += iconSpacing;
    }
    if (hunger.show) {
      bool crit = (hunger.color == COL_CRIT);
      if (!crit || blinkOn) drawBone(iconX, iconY, iconSize, hunger.color);
      iconX += iconSpacing;
    }
    if (hygiene.show) {
      bool crit = (hygiene.color == COL_CRIT);
      if (!crit || blinkOn) drawDrop(iconX, iconY, iconSize, hygiene.color);
      iconX += iconSpacing;
    }
  }

  g->draw16bitRGBBitmap(BAR_X, BAR_Y, s_barBuf, BAR_W, BAR_H);
  s_wasDrawn = anyToShow;

  expand(bbox, BAR_X, BAR_Y, BAR_W, BAR_H);
}

} // namespace OverlayStatus
