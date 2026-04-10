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

// Viewport : ne flush qu'une sous-region du FB (pour superposer face + LVGL)
// Coordonnees relatives au FB (pas a l'ecran)
void setViewport(int16_t x, int16_t y, int16_t w, int16_t h);
void clearViewport();

// Scale : reduit la taille du visage (1.0 = normal, 0.6 = 60%)
void setScale(float scale);
void resetScale();

} // namespace FaceRenderer

#endif
