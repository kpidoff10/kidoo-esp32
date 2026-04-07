#include "behavior_objects.h"
#include "../../config/config.h"
#include <lvgl.h>
#include <cmath>

namespace {

constexpr int16_t SCR_W = GOTCHI_LCD_WIDTH;
constexpr int16_t SCR_H = GOTCHI_LCD_HEIGHT;
constexpr int16_t SCR_CX = SCR_W / 2;
constexpr int16_t SCR_CY = SCR_H / 2;

struct VisualObject {
  lv_obj_t* obj = nullptr;
  float x = 0, y = 0, vx = 0, vy = 0;
  float gravity = 0, bounce = 0;
  bool trackEyes = false;
  bool alive = false;
  uint32_t lifetime = 0;  // 0 = pas d'auto-destroy
  uint32_t age = 0;
};

VisualObject s_pool[MAX_VISUAL_OBJECTS];

lv_obj_t* createObj(ObjectShape shape, uint32_t color, int16_t size) {
  lv_obj_t* obj = lv_obj_create(lv_screen_active());
  lv_obj_remove_style_all(obj);
  lv_obj_set_size(obj, size, shape == ObjectShape::Drop ? (int16_t)(size * 1.4f) : size);
  lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_clear_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));

  int16_t radius = 0;
  switch (shape) {
    case ObjectShape::Circle: radius = size / 2; break;
    case ObjectShape::Drop:   radius = size / 3; break;
    case ObjectShape::Rect:   radius = size / 6; break;
  }
  lv_obj_set_style_radius(obj, radius, 0);

  // Petit glow
  lv_obj_set_style_shadow_color(obj, lv_color_hex(color), 0);
  lv_obj_set_style_shadow_width(obj, 8, 0);
  lv_obj_set_style_shadow_spread(obj, 2, 0);
  lv_obj_set_style_shadow_opa(obj, LV_OPA_40, 0);

  return obj;
}

} // namespace

namespace BehaviorObjects {

void init() {
  for (auto& o : s_pool) {
    o.alive = false;
    o.obj = nullptr;
  }
}

void update(uint32_t dtMs) {
  float dt = (float)dtMs;

  for (auto& o : s_pool) {
    if (!o.alive) continue;

    o.age += dtMs;

    // Auto-destroy
    if (o.lifetime > 0 && o.age >= o.lifetime) {
      if (o.obj) lv_obj_add_flag(o.obj, LV_OBJ_FLAG_HIDDEN);
      o.alive = false;
      continue;
    }

    // Physique
    o.vy += o.gravity * dt;
    o.x += o.vx * dt;
    o.y += o.vy * dt;

    // Rebond au sol (380px = bas de l'écran rond)
    if (o.bounce > 0 && o.y > 380.0f) {
      o.y = 380.0f;
      o.vy = -fabsf(o.vy) * o.bounce;
    }

    // Murs
    if (o.x < 30.0f)  { o.x = 30.0f;  o.vx = fabsf(o.vx); }
    if (o.x > SCR_W - 30.0f) { o.x = SCR_W - 30.0f; o.vx = -fabsf(o.vx); }

    // Hors écran → destroy
    if (o.y < -50 || o.y > SCR_H + 50) {
      if (o.obj) lv_obj_add_flag(o.obj, LV_OBJ_FLAG_HIDDEN);
      o.alive = false;
      continue;
    }

    // Position LVGL
    if (o.obj) {
      lv_obj_set_pos(o.obj, (int16_t)o.x - lv_obj_get_width(o.obj) / 2,
                             (int16_t)o.y - lv_obj_get_height(o.obj) / 2);
    }
  }
}

int spawn(ObjectShape shape, uint32_t color, int16_t size,
          float x, float y, float vx, float vy,
          float gravity, float bounce, bool trackEyes, uint32_t lifetimeMs) {
  for (int i = 0; i < MAX_VISUAL_OBJECTS; i++) {
    if (!s_pool[i].alive) {
      auto& o = s_pool[i];
      if (!o.obj) {
        o.obj = createObj(shape, color, size);
      } else {
        // Réutiliser l'objet existant
        lv_obj_clear_flag(o.obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(o.obj, lv_color_hex(color), 0);
        lv_obj_set_style_shadow_color(o.obj, lv_color_hex(color), 0);
        int16_t h = shape == ObjectShape::Drop ? (int16_t)(size * 1.4f) : size;
        lv_obj_set_size(o.obj, size, h);
        int16_t radius = shape == ObjectShape::Circle ? size / 2 : (shape == ObjectShape::Drop ? size / 3 : size / 6);
        lv_obj_set_style_radius(o.obj, radius, 0);
      }
      o.x = x; o.y = y; o.vx = vx; o.vy = vy;
      o.gravity = gravity; o.bounce = bounce;
      o.trackEyes = trackEyes;
      o.alive = true;
      o.lifetime = lifetimeMs;
      o.age = 0;
      return i;
    }
  }
  return -1; // Pool plein
}

void destroy(int id) {
  if (id < 0 || id >= MAX_VISUAL_OBJECTS) return;
  if (s_pool[id].obj) lv_obj_add_flag(s_pool[id].obj, LV_OBJ_FLAG_HIDDEN);
  s_pool[id].alive = false;
}

void destroyAll() {
  for (auto& o : s_pool) {
    if (o.obj) lv_obj_add_flag(o.obj, LV_OBJ_FLAG_HIDDEN);
    o.alive = false;
  }
}

bool getLookTarget(float& outX, float& outY) {
  for (const auto& o : s_pool) {
    if (o.alive && o.trackEyes) {
      outX = (o.x - SCR_CX) / (SCR_W * 0.4f);
      outY = (o.y - SCR_CY) / (SCR_H * 0.4f);
      if (outX > 1.0f) outX = 1.0f;
      if (outX < -1.0f) outX = -1.0f;
      if (outY > 1.0f) outY = 1.0f;
      if (outY < -1.0f) outY = -1.0f;
      return true;
    }
  }
  return false;
}

} // namespace BehaviorObjects
