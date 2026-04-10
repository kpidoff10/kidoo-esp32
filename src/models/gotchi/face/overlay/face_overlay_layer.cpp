#include "face_overlay_layer.h"
#include "overlay_common.h"
#include "overlay_anger.h"
#include "overlay_sleep_zzz.h"
// overlay_status desactive — les stats sont dans la view stats

using namespace OverlayCommon;

namespace {

bool s_mangaCross = false;
bool s_sleepZzz   = false;
Arduino_GFX* s_gfx = nullptr;

} // namespace

namespace FaceOverlayLayer {

void init() {
  s_mangaCross = false;
  s_sleepZzz   = false;
  OverlayAnger::reset();
  OverlaySleepZzz::reset();
}

void update(uint32_t dtMs) {
  float sec = dtMs / 1000.0f;
  if (s_mangaCross) OverlayAnger::update(sec);
  if (s_sleepZzz)   OverlaySleepZzz::update(sec);
}

void draw(Arduino_GFX* gfx) {
  if (!gfx) return;

  s_gfx = gfx;

  BBox cur{};
  if (s_mangaCross) OverlayAnger::draw(gfx, cur);
  if (s_sleepZzz)   OverlaySleepZzz::draw(gfx, cur);
}

void setMangaCross(bool enabled) {
  if (s_mangaCross && !enabled && s_gfx) {
    OverlayAnger::clear(s_gfx);
  }
  s_mangaCross = enabled;
  if (!enabled) OverlayAnger::reset();
}

void setSleepZzz(bool enabled) {
  if (s_sleepZzz && !enabled && s_gfx) {
    OverlaySleepZzz::clear(s_gfx);
  }
  s_sleepZzz = enabled;
  if (!enabled) OverlaySleepZzz::reset();
}

} // namespace FaceOverlayLayer
