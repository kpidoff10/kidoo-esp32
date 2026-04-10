#include "behavior_objects.h"
#include "../../config/config.h"
#include "../face_renderer.h"
#include "sprites/sprite_heart_24.h"
#include <Arduino_GFX_Library.h>
#include <esp_heap_caps.h>
#include <pgmspace.h>
#include <cmath>
#include <cstring>

extern Arduino_GFX* getGotchiGfx();

namespace {

constexpr int16_t SCR_W = GOTCHI_LCD_WIDTH;
constexpr int16_t SCR_H = GOTCHI_LCD_HEIGHT;
constexpr int16_t SCR_CX = SCR_W / 2;
constexpr int16_t SCR_CY = SCR_H / 2;

// Convertir couleur RGB888 (uint32_t 0xRRGGBB) en RGB565
inline uint16_t toRgb565(uint32_t c) {
  uint8_t r = (c >> 16) & 0xFF;
  uint8_t g = (c >> 8) & 0xFF;
  uint8_t b = c & 0xFF;
  return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

struct VisualObject {
  float x = 0, y = 0, vx = 0, vy = 0;
  float gravity = 0, bounce = 0;
  uint16_t color565 = 0;
  int16_t size = 0;
  ObjectShape shape = ObjectShape::Circle;
  bool trackEyes = false;
  bool alive = false;
  bool held = false;  // Tenu par le doigt (pas de physique)
  uint32_t lifetime = 0;
  uint32_t age = 0;
};

VisualObject s_pool[MAX_VISUAL_OBJECTS];

// Dessiner un pixel dans le framebuffer (coordonnees ecran → FB locales)
inline void fbPx(uint16_t* fb, int16_t fbW, int16_t fbH, int16_t fbX, int16_t fbY,
                 int16_t sx, int16_t sy, uint16_t color) {
  int16_t lx = sx - fbX;
  int16_t ly = sy - fbY;
  if (lx >= 0 && lx < fbW && ly >= 0 && ly < fbH)
    fb[ly * fbW + lx] = color;
}

// Dessiner un cercle plein dans le FB
void drawCircle(uint16_t* fb, int16_t fbW, int16_t fbH, int16_t fbX, int16_t fbY,
                int16_t cx, int16_t cy, int16_t r, uint16_t color) {
  for (int16_t dy = -r; dy <= r; dy++) {
    int16_t dx = (int16_t)sqrtf((float)(r * r - dy * dy));
    for (int16_t x = cx - dx; x <= cx + dx; x++) {
      fbPx(fb, fbW, fbH, fbX, fbY, x, cy + dy, color);
    }
  }
}

// Dessiner un rectangle plein dans le FB
void drawRect(uint16_t* fb, int16_t fbW, int16_t fbH, int16_t fbX, int16_t fbY,
              int16_t cx, int16_t cy, int16_t w, int16_t h, uint16_t color) {
  for (int16_t dy = -h / 2; dy < h / 2; dy++) {
    for (int16_t dx = -w / 2; dx < w / 2; dx++) {
      fbPx(fb, fbW, fbH, fbX, fbY, cx + dx, cy + dy, color);
    }
  }
}

// Mélange une couleur source 565 sur un fond 565 selon alpha (0..255)
inline uint16_t blend565(uint16_t bg, uint16_t fg, uint8_t alpha) {
  if (alpha == 0) return bg;
  if (alpha >= 250) return fg;
  uint16_t br = (bg >> 11) & 0x1F;
  uint16_t bgn = (bg >> 5) & 0x3F;
  uint16_t bb = bg & 0x1F;
  uint16_t fr = (fg >> 11) & 0x1F;
  uint16_t fgn = (fg >> 5) & 0x3F;
  uint16_t fb_ = fg & 0x1F;
  uint16_t inv = 255 - alpha;
  uint16_t r = (fr * alpha + br * inv) / 255;
  uint16_t g = (fgn * alpha + bgn * inv) / 255;
  uint16_t b = (fb_ * alpha + bb * inv) / 255;
  return (r << 11) | (g << 5) | b;
}

// Blit du sprite heart_24 (alpha map PROGMEM) avec scaling nearest et alpha-blend
void drawHeart(uint16_t* fb, int16_t fbW, int16_t fbH, int16_t fbX, int16_t fbY,
               int16_t cx, int16_t cy, int16_t size, uint16_t color) {
  // size historique 12..19 → display ~24..38 (cœur visible ~14..22 px)
  int16_t dst = size * 2;
  if (dst < 8) dst = 8;
  const int16_t srcW = SPRITE_HEART_24_WIDTH;
  const int16_t srcH = SPRITE_HEART_24_HEIGHT;
  int16_t x0 = cx - dst / 2;
  int16_t y0 = cy - dst / 2;
  for (int16_t dy = 0; dy < dst; dy++) {
    int16_t sy = (dy * srcH) / dst;
    int16_t py = y0 + dy;
    int16_t ly = py - fbY;
    if (ly < 0 || ly >= fbH) continue;
    for (int16_t dx = 0; dx < dst; dx++) {
      int16_t sx = (dx * srcW) / dst;
      uint8_t a = pgm_read_byte(&SPRITE_HEART_24_DATA[sy * srcW + sx]);
      if (a == 0) continue;
      int16_t px = x0 + dx;
      int16_t lx = px - fbX;
      if (lx < 0 || lx >= fbW) continue;
      uint16_t* p = &fb[ly * fbW + lx];
      *p = blend565(*p, color, a);
    }
  }
}


// Dessiner une goutte (cercle + triangle en bas)
void drawDrop(uint16_t* fb, int16_t fbW, int16_t fbH, int16_t fbX, int16_t fbY,
              int16_t cx, int16_t cy, int16_t size, uint16_t color) {
  int16_t r = size / 3;
  drawCircle(fb, fbW, fbH, fbX, fbY, cx, cy, r, color);
  // Pointe en bas
  for (int16_t dy = 0; dy < size / 2; dy++) {
    float u = (float)dy / (float)(size / 2);
    int16_t dx = (int16_t)((float)r * (1.0f - u));
    for (int16_t x = cx - dx; x <= cx + dx; x++) {
      fbPx(fb, fbW, fbH, fbX, fbY, x, cy + r + dy, color);
    }
  }
}

} // namespace

namespace BehaviorObjects {

void init() {
  for (auto& o : s_pool) {
    o.alive = false;
  }
}

static BehaviorObjects::BounceCallback s_bounceCb = nullptr;

void setBounceCallback(BounceCallback cb) {
  s_bounceCb = cb;
}

void update(uint32_t dtMs) {
  float dt = (float)dtMs;

  for (int i = 0; i < MAX_VISUAL_OBJECTS; i++) {
    auto& o = s_pool[i];
    if (!o.alive) continue;

    o.age += dtMs;

    // Auto-destroy
    if (o.lifetime > 0 && o.age >= o.lifetime) {
      o.alive = false;
      continue;
    }

    // Pas de physique si tenu
    if (o.held) continue;

    // Physique
    o.vy += o.gravity * dt;
    o.x += o.vx * dt;
    o.y += o.vy * dt;

    // Rebond au sol
    if (o.bounce > 0 && o.y > 380.0f) {
      o.y = 380.0f;
      o.vy = -fabsf(o.vy) * o.bounce;
      if (s_bounceCb) s_bounceCb((int)i);
    }

    // Murs
    if (o.x < 30.0f)  { o.x = 30.0f;  o.vx = fabsf(o.vx); }
    if (o.x > SCR_W - 30.0f) { o.x = SCR_W - 30.0f; o.vx = -fabsf(o.vx); }

    // Hors ecran
    if (o.y < -50 || o.y > SCR_H + 50) {
      o.alive = false;
      continue;
    }
  }
}

// Mini framebuffer pour objets au-dessus du FB principal (zone haute)
constexpr int16_t TOP_X = 20;
constexpr int16_t TOP_Y = 30;
constexpr int16_t TOP_W = 426;
constexpr int16_t TOP_H = 100;  // y=30 a y=130 (juste au-dessus du FB principal)
static uint16_t* s_topBuf = nullptr;
static bool s_topDirty = false;
static bool s_topWasDirty = false;  // etait dirty au frame precedent

inline void topPx(int16_t sx, int16_t sy, uint16_t color) {
  int16_t lx = sx - TOP_X;
  int16_t ly = sy - TOP_Y;
  if (lx >= 0 && lx < TOP_W && ly >= 0 && ly < TOP_H)
    s_topBuf[ly * TOP_W + lx] = color;
}

void topFillCircle(int16_t cx, int16_t cy, int16_t r, uint16_t color) {
  for (int16_t dy = -r; dy <= r; dy++) {
    int16_t dx = (int16_t)sqrtf((float)(r * r - dy * dy));
    for (int16_t x = cx - dx; x <= cx + dx; x++) {
      topPx(x, cy + dy, color);
    }
  }
}

void topDrawHeart(int16_t cx, int16_t cy, int16_t size, uint16_t color) {
  int16_t dst = size * 2;
  if (dst < 8) dst = 8;
  const int16_t srcW = SPRITE_HEART_24_WIDTH;
  const int16_t srcH = SPRITE_HEART_24_HEIGHT;
  int16_t x0 = cx - dst / 2;
  int16_t y0 = cy - dst / 2;
  for (int16_t dy = 0; dy < dst; dy++) {
    int16_t sy = (dy * srcH) / dst;
    int16_t ly = (y0 + dy) - TOP_Y;
    if (ly < 0 || ly >= TOP_H) continue;
    for (int16_t dx = 0; dx < dst; dx++) {
      int16_t sx = (dx * srcW) / dst;
      uint8_t a = pgm_read_byte(&SPRITE_HEART_24_DATA[sy * srcW + sx]);
      if (a == 0) continue;
      int16_t lx = (x0 + dx) - TOP_X;
      if (lx < 0 || lx >= TOP_W) continue;
      uint16_t* p = &s_topBuf[ly * TOP_W + lx];
      *p = blend565(*p, color, a);
    }
  }
}

void draw() {
  uint16_t* fb = FaceRenderer::getNextBuffer();
  if (!fb) return;

  int16_t fbX = FaceRenderer::getFbX();
  int16_t fbY = FaceRenderer::getFbY();
  int16_t fbW = FaceRenderer::getFbW();
  int16_t fbH = FaceRenderer::getFbH();
  Arduino_GFX* gfx = getGotchiGfx();

  // Allouer le top buffer au premier appel
  if (!s_topBuf) {
    s_topBuf = (uint16_t*)heap_caps_malloc(TOP_W * TOP_H * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    if (!s_topBuf) return;
  }

  // Clear le top buffer
  memset(s_topBuf, 0, TOP_W * TOP_H * sizeof(uint16_t));
  s_topDirty = false;

  for (int i = 0; i < MAX_VISUAL_OBJECTS; i++) {
    const auto& o = s_pool[i];
    if (!o.alive) continue;

    int16_t sx = (int16_t)o.x;
    int16_t sy = (int16_t)o.y;
    int16_t r = o.size / 2;

    // Dessiner dans le FB principal (zone basse y=130-400)
    switch (o.shape) {
      case ObjectShape::Circle:
        drawCircle(fb, fbW, fbH, fbX, fbY, sx, sy, r, o.color565);
        break;
      case ObjectShape::Rect:
        drawRect(fb, fbW, fbH, fbX, fbY, sx, sy, o.size, o.size, o.color565);
        break;
      case ObjectShape::Drop:
        drawDrop(fb, fbW, fbH, fbX, fbY, sx, sy, o.size, o.color565);
        break;
      case ObjectShape::Heart:
        drawHeart(fb, fbW, fbH, fbX, fbY, sx, sy, o.size, o.color565);
        break;
    }

    // Aussi dessiner dans le top buffer (zone haute y=30-130)
    if (o.shape == ObjectShape::Heart) {
      if (sy - o.size < fbY) {
        topDrawHeart(sx, sy, o.size, o.color565);
        s_topDirty = true;
      }
    } else if (sy - r < fbY) {
      topFillCircle(sx, sy, r, o.color565);
      s_topDirty = true;
    }
  }

  // Flush le top buffer si dirty OU si c'etait dirty au frame precedent (pour nettoyer)
  if ((s_topDirty || s_topWasDirty) && gfx) {
    gfx->draw16bitRGBBitmap(TOP_X, TOP_Y, s_topBuf, TOP_W, TOP_H);
  }
  s_topWasDirty = s_topDirty;
}

int spawn(ObjectShape shape, uint32_t color, int16_t size,
          float x, float y, float vx, float vy,
          float gravity, float bounce, bool trackEyes, uint32_t lifetimeMs) {
  for (int i = 0; i < MAX_VISUAL_OBJECTS; i++) {
    if (!s_pool[i].alive) {
      auto& o = s_pool[i];
      o.shape = shape;
      o.color565 = toRgb565(color);
      o.size = size;
      o.x = x; o.y = y; o.vx = vx; o.vy = vy;
      o.gravity = gravity; o.bounce = bounce;
      o.trackEyes = trackEyes;
      o.alive = true;
      o.held = false;
      o.lifetime = lifetimeMs;
      o.age = 0;
      return i;
    }
  }
  return -1;
}

void destroy(int id) {
  if (id < 0 || id >= MAX_VISUAL_OBJECTS) return;
  s_pool[id].alive = false;
}

void destroyAll() {
  for (auto& o : s_pool) {
    o.alive = false;
  }
}

void hold(int id, float x, float y) {
  if (id < 0 || id >= MAX_VISUAL_OBJECTS) return;
  auto& o = s_pool[id];
  if (!o.alive) return;
  o.held = true;
  o.x = x;
  o.y = y;
  o.vx = 0;
  o.vy = 0;
}

void release(int id, float vx, float vy) {
  if (id < 0 || id >= MAX_VISUAL_OBJECTS) return;
  auto& o = s_pool[id];
  if (!o.alive) return;
  o.held = false;
  o.vx = vx;
  o.vy = vy;
}

bool isHeld(int id) {
  if (id < 0 || id >= MAX_VISUAL_OBJECTS) return false;
  return s_pool[id].alive && s_pool[id].held;
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
