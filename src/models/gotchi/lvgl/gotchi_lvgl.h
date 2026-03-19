#ifndef GOTCHI_LVGL_H
#define GOTCHI_LVGL_H

#include <Arduino.h>

/**
 * LVGL + Arduino_GFX (CO5300 QSPI) + touch CST92xx (SensorLib), Gotchi / Waveshare 1.75".
 */
namespace GotchiLvgl {

bool init();
void update();
void testDisplay();

} // namespace GotchiLvgl

#endif
