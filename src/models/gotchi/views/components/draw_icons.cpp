#include "draw_icons.h"
#include <cmath>

namespace DrawIcons {

void heart(Arduino_GFX* g, int16_t cx, int16_t cy, int16_t size, uint16_t color) {
  int16_t r = size * 4 / 10;
  g->fillCircle(cx - r, cy - r / 2, r, color);
  g->fillCircle(cx + r, cy - r / 2, r, color);
  g->fillTriangle(cx - size + 2, cy + 1, cx + size - 2, cy + 1, cx, cy + size, color);
}

void sparkle(Arduino_GFX* g, int16_t cx, int16_t cy, int16_t size, uint16_t color) {
  int16_t t = size / 5;
  if (t < 2) t = 2;
  // Croix principale
  g->fillRect(cx - t / 2, cy - size, t, size * 2, color);
  g->fillRect(cx - size, cy - t / 2, size * 2, t, color);
  // Diagonales plus courtes
  int16_t d = size * 2 / 3;
  g->fillRect(cx + d - t / 2, cy - d - t / 2, t, t, color);
  g->fillRect(cx - d - t / 2, cy - d - t / 2, t, t, color);
  g->fillRect(cx + d - t / 2, cy + d - t / 2, t, t, color);
  g->fillRect(cx - d - t / 2, cy + d - t / 2, t, t, color);
  // Mini point
  g->fillCircle(cx + size + 3, cy - size + 2, 2, color);
}

void smiley(Arduino_GFX* g, int16_t cx, int16_t cy, int16_t size, uint16_t color) {
  // Cercle exterieur (2px epaisseur)
  g->drawCircle(cx, cy, size, color);
  g->drawCircle(cx, cy, size - 1, color);
  // Yeux
  g->fillCircle(cx - size / 3, cy - size / 4, size / 6 + 1, color);
  g->fillCircle(cx + size / 3, cy - size / 4, size / 6 + 1, color);
  // Sourire (arc epais)
  for (int a = 15; a < 165; a += 2) {
    float rad = a * 3.14159f / 180.0f;
    int16_t sx = cx + (int16_t)(cosf(rad) * (float)(size * 2 / 3));
    int16_t sy = cy + size / 6 + (int16_t)(sinf(rad) * (float)(size / 3));
    g->fillRect(sx, sy, 2, 2, color);
  }
}

void fork(Arduino_GFX* g, int16_t cx, int16_t cy, int16_t size, uint16_t color) {
  int16_t t = size / 5;
  if (t < 2) t = 2;
  int16_t top = cy - size;
  int16_t tineH = size;
  int16_t w = size * 2 / 3;
  // 3 dents
  g->fillRect(cx - w, top, t, tineH, color);
  g->fillRect(cx - t / 2, top, t, tineH, color);
  g->fillRect(cx + w - t, top, t, tineH, color);
  // Barre horizontale
  g->fillRect(cx - w, top + tineH - t, w * 2, t, color);
  // Manche
  g->fillRect(cx - t / 2, cy, t, size, color);
}

void drop(Arduino_GFX* g, int16_t cx, int16_t cy, int16_t size, uint16_t color) {
  int16_t r = size * 2 / 3;
  g->fillCircle(cx, cy + size / 4, r, color);
  g->fillTriangle(cx, cy - size, cx - r, cy + size / 4, cx + r, cy + size / 4, color);
}

void star(Arduino_GFX* g, int16_t cx, int16_t cy, int16_t size, uint16_t color) {
  // Etoile 5 branches
  for (int i = 0; i < 5; i++) {
    float a1 = (i * 72 - 90) * 3.14159f / 180.0f;
    float a2 = ((i + 2) * 72 - 90) * 3.14159f / 180.0f;
    int16_t x1 = cx + (int16_t)(cosf(a1) * size);
    int16_t y1 = cy + (int16_t)(sinf(a1) * size);
    int16_t x2 = cx + (int16_t)(cosf(a2) * size);
    int16_t y2 = cy + (int16_t)(sinf(a2) * size);
    g->drawLine(x1, y1, x2, y2, color);
    g->drawLine(x1 + 1, y1, x2 + 1, y2, color);
  }
}

} // namespace DrawIcons
