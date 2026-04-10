#ifndef DRAW_ARC_H
#define DRAW_ARC_H

#include <Arduino_GFX_Library.h>
#include <cstdint>

namespace DrawArc {

/// Dessine un arc epais.
/// cx, cy    : centre du cercle
/// radius    : rayon moyen de l'arc
/// thickness : epaisseur (pixels)
/// startDeg  : angle de depart (0 = droite, 90 = bas, -90 = haut)
/// sweepDeg  : angle de balayage (positif = sens horaire)
/// color     : couleur RGB565
void draw(Arduino_GFX* gfx, int16_t cx, int16_t cy,
          int16_t radius, int16_t thickness,
          float startDeg, float sweepDeg, uint16_t color);

/// Dessine un arc avec embouts arrondis (caps)
void drawRounded(Arduino_GFX* gfx, int16_t cx, int16_t cy,
                 int16_t radius, int16_t thickness,
                 float startDeg, float sweepDeg, uint16_t color);

} // namespace DrawArc

#endif
