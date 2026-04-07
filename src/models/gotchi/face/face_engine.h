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
void blink();
void setAutoMode(bool enabled);
void setMouthState(float state); // -1=ouverte, 0=fermée, 1=sourire

FaceExpression getCurrentExpression();

} // namespace FaceEngine

#endif
