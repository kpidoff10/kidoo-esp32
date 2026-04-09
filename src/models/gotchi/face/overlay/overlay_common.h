#ifndef OVERLAY_COMMON_H
#define OVERLAY_COMMON_H

#include <Arduino_GFX_Library.h>
#include <algorithm>
#include <cstdint>

#include "../../config/config.h"
#include "../../config/gotchi_theme.h"

namespace OverlayCommon {

constexpr uint16_t COL_BG    = 0x0000;
#define COL_CYAN (GotchiTheme::getColors().overlay)

struct BBox {
  int16_t x = 0, y = 0, w = 0, h = 0;
  bool valid = false;
};

inline void expand(BBox& u, int16_t x, int16_t y, int16_t w, int16_t h) {
  if (w <= 0 || h <= 0) return;
  if (!u.valid) {
    u.x = x; u.y = y; u.w = w; u.h = h; u.valid = true;
    return;
  }
  int16_t x2 = (int16_t)std::max((int)u.x + u.w, (int)x + w);
  int16_t y2 = (int16_t)std::max((int)u.y + u.h, (int)y + h);
  u.x = (int16_t)std::min((int)u.x, (int)x);
  u.y = (int16_t)std::min((int)u.y, (int)y);
  u.w = (int16_t)(x2 - u.x);
  u.h = (int16_t)(y2 - u.y);
}

inline void pad(BBox& b, int16_t p) {
  if (!b.valid) return;
  b.x -= p; b.y -= p; b.w += 2 * p; b.h += 2 * p;
  if (b.x < 0) b.x = 0;
  if (b.y < 0) b.y = 0;
  if (b.x + b.w > GOTCHI_LCD_WIDTH) b.w = GOTCHI_LCD_WIDTH - b.x;
  if (b.y + b.h > GOTCHI_LCD_HEIGHT) b.h = GOTCHI_LCD_HEIGHT - b.y;
}

} // namespace OverlayCommon

#endif
