#include "overlay_sleep_zzz.h"
#include <cmath>
#include <cstring>
#include <pgmspace.h>
#include <esp_heap_caps.h>
#include "../behavior/sprites/sprite_asset.h"
#include "sprites/sprite_zzz_emoji_48.h"
#include "sprites/sprite_zzz_emoji_36.h"
#include "sprites/sprite_zzz_emoji_24.h"

using namespace OverlayCommon;

namespace {

float s_animT = 0.0f;

// Mini framebuffer couvrant toute la zone au-dessus des yeux
constexpr int16_t OVL_X = 20;
constexpr int16_t OVL_Y = 60;
constexpr int16_t OVL_W = 426;
constexpr int16_t OVL_H = 70;   // y=60 a y=130 (connecte au FB principal)
uint16_t* s_ovlBuf = nullptr;

// Blend alpha d'un pixel source sur le buffer
inline uint16_t blend565(uint16_t bg, uint16_t fg, uint8_t a) {
  if (a == 0) return bg;
  if (a == 255) return fg;
  uint16_t bgR = (bg >> 11) & 0x1F, bgG = (bg >> 5) & 0x3F, bgB = bg & 0x1F;
  uint16_t fgR = (fg >> 11) & 0x1F, fgG = (fg >> 5) & 0x3F, fgB = fg & 0x1F;
  uint8_t inv = 255 - a;
  uint16_t r = (fgR * a + bgR * inv) / 255;
  uint16_t g = (fgG * a + bgG * inv) / 255;
  uint16_t b = (fgB * a + bgB * inv) / 255;
  return (r << 11) | (g << 5) | b;
}

// Dessiner un sprite avec alpha dans le buffer overlay (coords locales buffer)
void drawSprite(const SpriteAsset& s, int16_t cx, int16_t cy, float alpha) {
  const int16_t srcW = s.width;
  const int16_t srcH = s.height;
  const int16_t x0 = cx - srcW / 2;
  const int16_t y0 = cy - srcH / 2;
  const bool hasRgb = (s.rgb565 != nullptr);

  for (int16_t dy = 0; dy < srcH; dy++) {
    int16_t py = y0 + dy;
    if (py < 0 || py >= OVL_H) continue;
    const int rowOff = dy * srcW;
    for (int16_t dx = 0; dx < srcW; dx++) {
      uint8_t a = pgm_read_byte(&s.alpha[rowOff + dx]);
      if (a == 0) continue;
      int16_t px = x0 + dx;
      if (px < 0 || px >= OVL_W) continue;
      // Appliquer l'alpha global (fade in/out)
      uint8_t finalA = (uint8_t)((uint16_t)a * (uint16_t)(uint8_t)(alpha * 255.0f) / 255);
      if (finalA == 0) continue;
      uint16_t src = hasRgb ? pgm_read_word(&s.rgb565[rowOff + dx]) : COL_CYAN;
      uint16_t* p = &s_ovlBuf[py * OVL_W + px];
      *p = blend565(*p, src, finalA);
    }
  }
}

} // namespace

namespace OverlaySleepZzz {

void update(float dtSec) {
  s_animT += dtSec;
}

void draw(Arduino_GFX* g, BBox& bbox) {
  if (!s_ovlBuf) {
    s_ovlBuf = (uint16_t*)heap_caps_malloc(OVL_W * OVL_H * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    if (!s_ovlBuf) return;
  }

  memset(s_ovlBuf, 0, OVL_W * OVL_H * sizeof(uint16_t));

  // 3 emoji 💤 en cascade : grand -> moyen -> petit, flottent vers haut-droite
  constexpr float BASE_X = 200.0f;
  constexpr float BASE_Y = 58.0f;
  constexpr float RISE   = 50.0f;
  constexpr float DRIFT_X = 120.0f;
  constexpr float CYCLE  = 28.0f;
  constexpr int NUM = 3;

  const SpriteAsset* sprites[NUM] = {
    &SPRITE_ZZZ_EMOJI_48_ASSET,
    &SPRITE_ZZZ_EMOJI_36_ASSET,
    &SPRITE_ZZZ_EMOJI_24_ASSET,
  };

  for (int i = 0; i < NUM; i++) {
    float phase = fmodf(s_animT + (float)i * (CYCLE / (float)NUM), CYCLE) / CYCLE;

    float alpha = 1.0f;
    if (phase < 0.1f) alpha = phase / 0.1f;
    else if (phase > 0.75f) alpha = (1.0f - phase) / 0.25f;
    if (alpha < 0.15f) continue;

    float y = BASE_Y - phase * RISE;
    float x = BASE_X + phase * DRIFT_X + sinf(s_animT * 1.2f + (float)i * 2.5f) * 4.0f;

    drawSprite(*sprites[i], (int16_t)x, (int16_t)y, alpha);
  }

  g->draw16bitRGBBitmap(OVL_X, OVL_Y, s_ovlBuf, OVL_W, OVL_H);
  expand(bbox, OVL_X, OVL_Y, OVL_W, OVL_H);
}

void clear(Arduino_GFX* g) {
  if (s_ovlBuf && g) {
    memset(s_ovlBuf, 0, OVL_W * OVL_H * sizeof(uint16_t));
    g->draw16bitRGBBitmap(OVL_X, OVL_Y, s_ovlBuf, OVL_W, OVL_H);
  }
}

void reset() {
  s_animT = 0.0f;
}

} // namespace OverlaySleepZzz
