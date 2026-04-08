#ifndef OVERLAY_SLEEP_ZZZ_H
#define OVERLAY_SLEEP_ZZZ_H

#include "overlay_common.h"

namespace OverlaySleepZzz {

void update(float dtSec);
void draw(Arduino_GFX* g, OverlayCommon::BBox& bbox);
void clear(Arduino_GFX* g);
void reset();

} // namespace OverlaySleepZzz

#endif
