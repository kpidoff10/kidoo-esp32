#include "init_model.h"

#include "../lvgl/gotchi_lvgl.h"

bool InitModelGotchi::init() {
  return GotchiLvgl::init();
}

bool InitModelGotchi::configure() {
  return true;
}

void InitModelGotchi::update() {
  GotchiLvgl::update();
}
