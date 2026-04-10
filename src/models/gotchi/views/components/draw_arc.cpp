#include "draw_arc.h"
#include <cmath>

namespace DrawArc {

void draw(Arduino_GFX* gfx, int16_t cx, int16_t cy,
          int16_t radius, int16_t thickness,
          float startDeg, float sweepDeg, uint16_t color) {
  if (!gfx || thickness <= 0 || sweepDeg == 0) return;

  float rOuter = radius + thickness / 2.0f;
  float rInner = radius - thickness / 2.0f;
  if (rInner < 0) rInner = 0;

  // Parcourir chaque degre avec sub-stepping pour un rendu lisse
  float step = 0.5f;  // demi-degre pour lisser
  int steps = (int)(fabsf(sweepDeg) / step) + 1;
  float dir = (sweepDeg > 0) ? 1.0f : -1.0f;

  for (int s = 0; s <= steps; s++) {
    float deg = startDeg + dir * s * step;
    float rad = deg * 3.14159265f / 180.0f;
    float cosA = cosf(rad);
    float sinA = sinf(rad);

    // Dessiner la ligne radiale (epaisseur)
    for (float r = rInner; r <= rOuter; r += 1.0f) {
      int16_t px = cx + (int16_t)(cosA * r);
      int16_t py = cy + (int16_t)(sinA * r);
      gfx->writePixelPreclipped(px, py, color);
    }
  }
}

void drawRounded(Arduino_GFX* gfx, int16_t cx, int16_t cy,
                 int16_t radius, int16_t thickness,
                 float startDeg, float sweepDeg, uint16_t color) {
  if (!gfx || thickness <= 0 || sweepDeg == 0) return;

  // Dessiner l'arc principal avec startWrite/endWrite pour performance
  gfx->startWrite();
  draw(gfx, cx, cy, radius, thickness, startDeg, sweepDeg, color);

  // Embouts arrondis (cercles aux extremites)
  int16_t capR = thickness / 2;
  if (capR < 1) capR = 1;

  // Cap debut
  float radStart = startDeg * 3.14159265f / 180.0f;
  int16_t sx = cx + (int16_t)(cosf(radStart) * radius);
  int16_t sy = cy + (int16_t)(sinf(radStart) * radius);
  gfx->fillCircle(sx, sy, capR, color);

  // Cap fin
  float radEnd = (startDeg + sweepDeg) * 3.14159265f / 180.0f;
  int16_t ex = cx + (int16_t)(cosf(radEnd) * radius);
  int16_t ey = cy + (int16_t)(sinf(radEnd) * radius);
  gfx->fillCircle(ex, ey, capR, color);

  gfx->endWrite();
}

} // namespace DrawArc
