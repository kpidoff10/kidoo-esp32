#ifndef FACE_RENDERER_H
#define FACE_RENDERER_H

#include "face_config.h"
#include <lvgl.h>

/**
 * Rendu LVGL des yeux style EMO/Cozmo.
 * Deux rectangles arrondis blancs sur fond noir.
 */
namespace FaceRenderer {

void init();
void render(const EyeConfig& left, const EyeConfig& right, float lookX, float lookY, float mouthState = 0.0f);

} // namespace FaceRenderer

#endif
