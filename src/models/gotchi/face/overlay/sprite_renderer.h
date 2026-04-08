#ifndef SPRITE_RENDERER_H
#define SPRITE_RENDERER_H

#include <Arduino_GFX_Library.h>
#include <pgmspace.h>
#include <cstdint>

namespace SpriteRenderer {

/// Dessine un sprite alpha-map colorise a la position (x, y).
/// data    : tableau PROGMEM de bytes (alpha 0-255 par pixel)
/// w, h    : dimensions du sprite
/// color   : couleur RGB565 a appliquer (ex: 0x073F = cyan)
/// alpha_threshold : en dessous, pixel ignore (defaut 16 pour ignorer le bruit)
inline void draw(Arduino_GFX* gfx, int16_t x, int16_t y,
                 const uint8_t* data, uint16_t w, uint16_t h,
                 uint16_t color, uint8_t alpha_threshold = 16) {
  uint8_t r5 = (color >> 11) & 0x1F;
  uint8_t g6 = (color >> 5) & 0x3F;
  uint8_t b5 = color & 0x1F;

  gfx->startWrite();
  for (uint16_t row = 0; row < h; row++) {
    for (uint16_t col = 0; col < w; col++) {
      uint8_t a = pgm_read_byte(&data[row * w + col]);
      if (a < alpha_threshold) continue;

      uint16_t px_color;
      if (a >= 240) {
        px_color = color;
      } else {
        // Semi-transparent : attenuer la couleur (glow)
        uint8_t r = (r5 * a) >> 8;
        uint8_t g = (g6 * a) >> 8;
        uint8_t b = (b5 * a) >> 8;
        px_color = (r << 11) | (g << 5) | b;
      }
      gfx->writePixelPreclipped(x + col, y + row, px_color);
    }
  }
  gfx->endWrite();
}

} // namespace SpriteRenderer

#endif
