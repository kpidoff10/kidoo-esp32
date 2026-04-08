#ifndef OVERLAY_ANGER_H
#define OVERLAY_ANGER_H

#include "overlay_common.h"

namespace OverlayAnger {

void update(float dtSec);
void draw(Arduino_GFX* g, OverlayCommon::BBox& bbox);
void clear(Arduino_GFX* g);
void reset();

} // namespace OverlayAnger

#endif
