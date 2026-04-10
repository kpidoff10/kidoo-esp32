#ifndef OVERLAY_STATUS_H
#define OVERLAY_STATUS_H

#include "overlay_common.h"

namespace OverlayStatus {

void update(float dtSec);
void draw(Arduino_GFX* g, OverlayCommon::BBox& bbox);

} // namespace OverlayStatus

#endif
