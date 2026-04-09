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

// Acces au framebuffer pour dessiner des objets (behavior objects)
uint16_t* getNextBuffer();
int16_t getFbX();
int16_t getFbY();
int16_t getFbW();
int16_t getFbH();

} // namespace FaceRenderer

#endif
