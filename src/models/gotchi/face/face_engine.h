#ifndef FACE_ENGINE_H
#define FACE_ENGINE_H

#include "face_config.h"

/**
 * Moteur d'animation du visage Gotchi.
 * Gère les transitions, clignements automatiques et regard idle.
 */
namespace FaceEngine {

void init();
void update(uint32_t dtMs);

void setExpression(FaceExpression expr);
void lookAt(float x, float y);
void lookAtForced(float x, float y);  // Prioritaire, ignore auto/behavior pendant 200ms
void blink();
void setAutoMode(bool enabled);
void setMouthState(float state); // -1=ouverte, 0=fermée, 1=sourire

// Animations gestuelles
enum class GestureSpeed { Slow, Normal, Fast };
void nod(GestureSpeed speed = GestureSpeed::Normal);    // Oui
void shake(GestureSpeed speed = GestureSpeed::Normal);   // Non
bool isGesturePlaying();

// Trauma (secousse physique violente)
// dirX/dirY : direction normalisee de la secousse
void trauma(float dirX, float dirY);

FaceExpression getCurrentExpression();

} // namespace FaceEngine

#endif
