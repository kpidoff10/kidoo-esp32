#ifndef VIEW_MANAGER_H
#define VIEW_MANAGER_H

#include "view.h"
#include <cstdint>

namespace ViewManager {

void init(Arduino_GFX* gfx);
void update(uint32_t dtMs, Arduino_GFX* gfx);

// Appele par gotchi_lvgl quand un swipe horizontal est detecte
// Retourne true si le swipe a ete consomme (changement de page)
bool handleSwipe(int16_t dx, int16_t dy, Arduino_GFX* gfx);

const View* getCurrentView();
bool isFaceView();  // Raccourci : est-on sur la page visage ?

// Navigation par commande
void goTo(int index, Arduino_GFX* gfx);
void goToByName(const char* name, Arduino_GFX* gfx);

} // namespace ViewManager

#endif
