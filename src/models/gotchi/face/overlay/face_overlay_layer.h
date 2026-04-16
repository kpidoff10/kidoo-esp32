#ifndef FACE_OVERLAY_LAYER_H
#define FACE_OVERLAY_LAYER_H

#include <Arduino_GFX_Library.h>
#include <cstdint>

namespace FaceOverlayLayer {

void init();
void update(uint32_t dtMs);

/// Appeler apres FaceEngine::update. Efface bbox precedent puis redessine.
void draw(Arduino_GFX* gfx);

void setMangaCross(bool enabled);
void setSleepZzz(bool enabled);
bool isSleepZzz();

} // namespace FaceOverlayLayer

#endif
