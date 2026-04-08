#include "overlay_anger.h"
#include <cmath>
#include <cstring>
#include <esp_heap_caps.h>

using namespace OverlayCommon;

namespace {
constexpr uint16_t COL_ANGER = 0xF800;  // rouge vif
float s_wobble = 0.0f;

// Mini framebuffer pour le 💢
constexpr int16_t OVL_X = 270;
constexpr int16_t OVL_Y = 80;
constexpr int16_t OVL_W = 80;
constexpr int16_t OVL_H = 80;
uint16_t* s_ovlBuf = nullptr;

inline void bufSetPixel(int16_t x, int16_t y, uint16_t color) {
  if (x >= 0 && x < OVL_W && y >= 0 && y < OVL_H)
    s_ovlBuf[y * OVL_W + x] = color;
}

inline void bufFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  for (int16_t j = y; j < y + h; j++)
    for (int16_t i = x; i < x + w; i++)
      bufSetPixel(i, j, color);
}

} // namespace

namespace OverlayAnger {

void update(float dtSec) {
  s_wobble += dtSec * 10.0f;
}

void draw(Arduino_GFX* g, BBox& bbox) {
  if (!s_ovlBuf) {
    s_ovlBuf = (uint16_t*)heap_caps_malloc(OVL_W * OVL_H * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    if (!s_ovlBuf) return;
  }

  memset(s_ovlBuf, 0, OVL_W * OVL_H * sizeof(uint16_t));

  // Pulse lent
  float pulse = 0.8f + 0.2f * sinf(s_wobble * 1.2f);

  int16_t arm = (int16_t)(16.0f * pulse);
  int16_t t   = (int16_t)(7.0f * pulse);
  int16_t gap = (int16_t)(4.0f * pulse);
  int16_t off = (int16_t)(4.0f * pulse);
  if (t < 3) t = 3;
  if (arm < 6) arm = 6;
  if (gap < 2) gap = 2;

  // Centre du buffer + wobble
  int16_t cx = OVL_W / 2 + (int16_t)(sinf(s_wobble) * 2.0f);
  int16_t cy = OVL_H / 2 + (int16_t)(sinf(s_wobble * 1.5f) * 1.5f);

  // 4 branches decalees (pinwheel)
  bufFillRect(cx - t / 2 - off, cy - gap - arm, t, arm, COL_ANGER);
  bufFillRect(cx - t / 2 + off, cy + gap,       t, arm, COL_ANGER);
  bufFillRect(cx - gap - arm, cy - t / 2 + off, arm, t, COL_ANGER);
  bufFillRect(cx + gap,       cy - t / 2 - off, arm, t, COL_ANGER);

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
  s_wobble = 0.0f;
}

} // namespace OverlayAnger
