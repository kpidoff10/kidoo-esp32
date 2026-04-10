#include "view_face.h"
#include "../../config/config.h"
#include "../../config/gotchi_theme.h"
#include "../../face/face_engine.h"
#include "../../face/overlay/face_overlay_layer.h"
#include "../../face/behavior/behavior_engine.h"
#include "../../imu/gotchi_imu.h"

static void faceInit() {
  // Deja initialise dans gotchi_lvgl::init()
}

static void faceUpdate(uint32_t dtMs, Arduino_GFX* gfx) {
  // IMU shake
  float shakeX = 0, shakeY = 0;
  if (GotchiImu::update(dtMs, shakeX, shakeY)) {
    FaceEngine::trauma(shakeX, shakeY);
    BehaviorEngine::onShake();
  }

  BehaviorEngine::update(dtMs);
  FaceEngine::update(dtMs);
  FaceOverlayLayer::update(dtMs);
  FaceOverlayLayer::draw(gfx);
}

static void drawPageDots(Arduino_GFX* gfx, int active) {
  if (!gfx) return;
  constexpr int16_t cx = GOTCHI_LCD_WIDTH / 2;
  constexpr int16_t y = 450;
  uint16_t eyeCol = GotchiTheme::getColors().eye;
  for (int i = 0; i < 3; i++) {
    int16_t x = cx - 18 + i * 18;
    gfx->fillRoundRect(x - 7, y, 14, 4, 2, i == active ? eyeCol : 0x2104);
  }
}

static void faceOnEnter(Arduino_GFX* gfx) {
  drawPageDots(gfx, 1);  // Face = page du milieu
}

static void faceOnExit(Arduino_GFX* gfx) {
  if (gfx) gfx->fillScreen(0x0000);
}

const View VIEW_FACE = {
  "face", faceInit, faceUpdate, faceOnEnter, faceOnExit, false
};
