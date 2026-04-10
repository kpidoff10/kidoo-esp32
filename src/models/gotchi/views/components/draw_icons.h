#ifndef DRAW_ICONS_H
#define DRAW_ICONS_H

#include <Arduino_GFX_Library.h>
#include <cstdint>

namespace DrawIcons {

void heart(Arduino_GFX* g, int16_t cx, int16_t cy, int16_t size, uint16_t color);
void sparkle(Arduino_GFX* g, int16_t cx, int16_t cy, int16_t size, uint16_t color);
void smiley(Arduino_GFX* g, int16_t cx, int16_t cy, int16_t size, uint16_t color);
void fork(Arduino_GFX* g, int16_t cx, int16_t cy, int16_t size, uint16_t color);
void drop(Arduino_GFX* g, int16_t cx, int16_t cy, int16_t size, uint16_t color);
void star(Arduino_GFX* g, int16_t cx, int16_t cy, int16_t size, uint16_t color);

} // namespace DrawIcons

#endif
