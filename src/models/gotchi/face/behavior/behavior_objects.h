#ifndef BEHAVIOR_OBJECTS_H
#define BEHAVIOR_OBJECTS_H

#include <cstdint>

struct SpriteAsset;

enum class ObjectShape : uint8_t { Circle, Rect, Drop, Sprite };

constexpr int MAX_VISUAL_OBJECTS = 8;

namespace BehaviorObjects {

void init();
void update(uint32_t dtMs);
void draw();  // Dessiner les objets dans le framebuffer (appeler apres FaceRenderer::render)

// Spawn un objet, retourne son ID (0 à MAX-1) ou -1 si pool plein
int spawn(ObjectShape shape, uint32_t color, int16_t size,
          float x, float y, float vx, float vy,
          float gravity, float bounce, bool trackEyes, uint32_t lifetimeMs);

// Spawn un sprite (alpha-only ou RGBA, déterminé par l'asset).
// Pour un sprite alpha-only, color sert à coloriser le sprite (jaune, blanc...).
// Pour un sprite RGBA, color est ignoré (les couleurs natives sont préservées).
int spawnSprite(const SpriteAsset& asset, uint32_t color,
                float x, float y, float vx, float vy,
                float gravity, float bounce, bool trackEyes, uint32_t lifetimeMs);

void destroy(int id);
void destroyAll();

// Hold: attacher un objet au doigt (desactive physique)
void hold(int id, float x, float y);
// Release: lacher avec velocite
void release(int id, float vx, float vy);
// Check si un objet est tenu
bool isHeld(int id);

// Retourne true si un objet avec trackEyes existe, écrit sa position normalisée
bool getLookTarget(float& outX, float& outY);

// Callback appelé à chaque rebond d'un objet sur le sol (collision physique).
// Utilisé pour brancher du feedback (haptique, son, particules).
// nullptr = pas de callback. Un seul callback global.
using BounceCallback = void(*)(int objId);
void setBounceCallback(BounceCallback cb);

// Callback pour dessiner du contenu custom dans le top buffer (426x100px).
// Appelé dans draw() après les sprites, avant le flush à l'écran.
using TopDrawCallback = void(*)(uint16_t* topBuf, int16_t topW, int16_t topH);
void setTopDrawCallback(TopDrawCallback cb);

} // namespace BehaviorObjects

#endif
