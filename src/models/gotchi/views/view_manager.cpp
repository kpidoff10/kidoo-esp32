#include "view_manager.h"
#include "face/view_face.h"
#include "stats/view_stats.h"
#include "settings/view_settings.h"
#include <Arduino.h>
#include <lvgl.h>
#include <cstring>
#include <cstdlib>

namespace {

// Ordre des pages : Stats ← Face → Settings
const View* ALL_VIEWS[] = {
  &VIEW_STATS,
  &VIEW_FACE,
  &VIEW_SETTINGS,
};
constexpr int VIEW_COUNT = sizeof(ALL_VIEWS) / sizeof(ALL_VIEWS[0]);
constexpr int DEFAULT_VIEW = 1;  // Face au centre

int s_currentIdx = DEFAULT_VIEW;

void switchTo(int idx, Arduino_GFX* gfx) {
  if (idx < 0 || idx >= VIEW_COUNT) return;
  if (idx == s_currentIdx) return;

  // Exit current
  if (ALL_VIEWS[s_currentIdx]->onExit) {
    ALL_VIEWS[s_currentIdx]->onExit(gfx);
  }

  int prevIdx = s_currentIdx;
  s_currentIdx = idx;

  Serial.printf("[VIEW] %s -> %s\n", ALL_VIEWS[prevIdx]->name, ALL_VIEWS[idx]->name);

  // Enter new
  if (ALL_VIEWS[s_currentIdx]->onEnter) {
    ALL_VIEWS[s_currentIdx]->onEnter(gfx);
  }
}

} // namespace

namespace ViewManager {

void init(Arduino_GFX* gfx) {
  s_currentIdx = DEFAULT_VIEW;
  for (int i = 0; i < VIEW_COUNT; i++) {
    if (ALL_VIEWS[i]->init) ALL_VIEWS[i]->init();
  }
  // Enter la view par defaut
  if (ALL_VIEWS[s_currentIdx]->onEnter) {
    ALL_VIEWS[s_currentIdx]->onEnter(gfx);
  }
}

void update(uint32_t dtMs, Arduino_GFX* gfx) {
  // LVGL d'abord (rend les arcs etc.) pour que GFX puisse dessiner par-dessus
  if (ALL_VIEWS[s_currentIdx]->usesLvgl) {
    lv_timer_handler();
  }
  if (ALL_VIEWS[s_currentIdx]->update) {
    ALL_VIEWS[s_currentIdx]->update(dtMs, gfx);
  }
}

bool handleSwipe(int16_t dx, int16_t dy, Arduino_GFX* gfx) {
  // Seulement les swipes principalement horizontaux
  int16_t adx = dx > 0 ? dx : -dx;
  int16_t ady = dy > 0 ? dy : -dy;
  if (adx < ady * 2) return false;  // Trop vertical, pas un changement de page

  if (dx < -40) {
    // Swipe gauche → page suivante (droite)
    if (s_currentIdx < VIEW_COUNT - 1) {
      switchTo(s_currentIdx + 1, gfx);
      return true;
    }
  } else if (dx > 40) {
    // Swipe droite → page precedente (gauche)
    if (s_currentIdx > 0) {
      switchTo(s_currentIdx - 1, gfx);
      return true;
    }
  }
  return false;
}

const View* getCurrentView() {
  return ALL_VIEWS[s_currentIdx];
}

bool isFaceView() {
  return s_currentIdx == DEFAULT_VIEW;
}

void goTo(int index, Arduino_GFX* gfx) {
  switchTo(index, gfx);
}

void goToByName(const char* name, Arduino_GFX* gfx) {
  for (int i = 0; i < VIEW_COUNT; i++) {
    if (strcmp(ALL_VIEWS[i]->name, name) == 0) {
      switchTo(i, gfx);
      return;
    }
  }
}

} // namespace ViewManager
