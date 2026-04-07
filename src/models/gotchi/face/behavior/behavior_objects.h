#ifndef BEHAVIOR_OBJECTS_H
#define BEHAVIOR_OBJECTS_H

#include <cstdint>

enum class ObjectShape : uint8_t { Circle, Rect, Drop };

constexpr int MAX_VISUAL_OBJECTS = 8;

namespace BehaviorObjects {

void init();
void update(uint32_t dtMs);

// Spawn un objet, retourne son ID (0 à MAX-1) ou -1 si pool plein
int spawn(ObjectShape shape, uint32_t color, int16_t size,
          float x, float y, float vx, float vy,
          float gravity, float bounce, bool trackEyes, uint32_t lifetimeMs);

void destroy(int id);
void destroyAll();

// Retourne true si un objet avec trackEyes existe, écrit sa position normalisée
bool getLookTarget(float& outX, float& outY);

} // namespace BehaviorObjects

#endif
