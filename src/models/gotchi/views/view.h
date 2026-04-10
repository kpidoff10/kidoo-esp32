#ifndef GOTCHI_VIEW_H
#define GOTCHI_VIEW_H

#include <Arduino_GFX_Library.h>
#include <cstdint>

struct View {
  const char* name;
  void (*init)();
  void (*update)(uint32_t dtMs, Arduino_GFX* gfx);
  void (*onEnter)(Arduino_GFX* gfx);
  void (*onExit)(Arduino_GFX* gfx);
  bool usesLvgl;  // true = appeler lv_timer_handler(), false = GFX direct
};

#endif
