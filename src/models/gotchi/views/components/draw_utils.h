#ifndef DRAW_UTILS_H
#define DRAW_UTILS_H

#include <cstdint>

namespace DrawUtils {

// Couleurs par niveau
constexpr uint16_t COL_GREEN  = 0x47E0;
constexpr uint16_t COL_YELLOW = 0xFE20;
constexpr uint16_t COL_RED    = 0xF800;
constexpr uint16_t COL_TRACK  = 0x1082;  // Gris tres sombre (fond arc)
constexpr uint16_t COL_DIM    = 0x4A69;  // Texte gris

inline uint16_t colorForPct(float pct) {
  if (pct >= 0.6f) return COL_GREEN;
  if (pct >= 0.3f) return COL_YELLOW;
  return COL_RED;
}

inline float statToPct(float val) {
  if (val < 0) return 0;
  if (val > 100) return 1.0f;
  return val / 100.0f;
}

} // namespace DrawUtils

#endif
