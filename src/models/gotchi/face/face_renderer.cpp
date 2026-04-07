#include "face_renderer.h"
#include "../config/config.h"

namespace {

constexpr int16_t SCR_CX = GOTCHI_LCD_WIDTH / 2;
constexpr int16_t SCR_CY = GOTCHI_LCD_HEIGHT / 2;
constexpr int16_t EYE_GAP = 190;
constexpr int16_t LEFT_EYE_CX  = SCR_CX - EYE_GAP / 2;
constexpr int16_t RIGHT_EYE_CX = SCR_CX + EYE_GAP / 2;
constexpr int16_t EYE_CY = SCR_CY;
constexpr int16_t LOOK_RANGE_X = 40;
constexpr int16_t LOOK_RANGE_Y = 25;
constexpr int16_t MOUTH_CX = SCR_CX;
constexpr int16_t MOUTH_CY = SCR_CY + 85;

lv_obj_t* s_leftEye = nullptr;
lv_obj_t* s_rightEye = nullptr;
lv_obj_t* s_mouth = nullptr;

lv_obj_t* makeObj(uint32_t color) {
  lv_obj_t* obj = lv_obj_create(lv_screen_active());
  lv_obj_remove_style_all(obj);
  lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_clear_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
  return obj;
}

void initEye(lv_obj_t*& eye) {
  eye = makeObj(0x00E5FF);
  lv_obj_set_style_bg_grad_color(eye, lv_color_hex(0x006878), 0);
  lv_obj_set_style_bg_grad_dir(eye, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_shadow_color(eye, lv_color_hex(0x00E5FF), 0);
  lv_obj_set_style_shadow_opa(eye, LV_OPA_50, 0);
}

void initMouth() {
  s_mouth = makeObj(0x00E5FF);
  lv_obj_set_style_bg_grad_color(s_mouth, lv_color_hex(0x006878), 0);
  lv_obj_set_style_bg_grad_dir(s_mouth, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_shadow_color(s_mouth, lv_color_hex(0x00E5FF), 0);
  lv_obj_set_style_shadow_opa(s_mouth, LV_OPA_30, 0);
  lv_obj_set_style_shadow_width(s_mouth, 6, 0);
  lv_obj_set_style_shadow_spread(s_mouth, 2, 0);
}

void applyEye(lv_obj_t* eye, const EyeConfig& cfg, int16_t baseCX, int16_t baseCY,
              float lookX, float lookY) {
  int16_t w = cfg.width > 0 ? cfg.width : 1;
  int16_t h = cfg.height > 0 ? cfg.height : 1;

  int16_t cx = baseCX + cfg.offsetX + (int16_t)(lookX * LOOK_RANGE_X);
  int16_t cy = baseCY + cfg.offsetY + (int16_t)(lookY * LOOK_RANGE_Y);

  lv_obj_set_pos(eye, cx - w / 2, cy - h / 2);
  lv_obj_set_size(eye, w, h);

  int16_t radius = (cfg.radiusTop + cfg.radiusBottom) / 2;
  int16_t maxR = (w < h ? w : h) / 2;
  if (radius > maxR) radius = maxR;
  lv_obj_set_style_radius(eye, radius, 0);

  int16_t minDim = w < h ? w : h;
  int16_t shadowW = minDim / 8;
  if (shadowW < 2) shadowW = 2;
  if (shadowW > 18) shadowW = 18;
  lv_obj_set_style_shadow_width(eye, shadowW, 0);
  lv_obj_set_style_shadow_spread(eye, shadowW / 4, 0);
}

// Paupières noires rotatées pour le slope
struct LidObjects {
  lv_obj_t* top;
  lv_obj_t* bot;
};

LidObjects s_leftLid, s_rightLid;

lv_obj_t* makeLid() {
  lv_obj_t* obj = makeObj(0x000000);
  lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
  return obj;
}

void applyLid(LidObjects& lid, const EyeConfig& cfg, int16_t baseCX, int16_t baseCY,
              float lookX, float lookY) {
  int16_t w = cfg.width > 0 ? cfg.width : 1;
  int16_t h = cfg.height > 0 ? cfg.height : 1;
  int16_t cx = baseCX + cfg.offsetX + (int16_t)(lookX * LOOK_RANGE_X);
  int16_t cy = baseCY + cfg.offsetY + (int16_t)(lookY * LOOK_RANGE_Y);

  if (cfg.slopeTop != 0.0f) {
    int16_t lidH = (int16_t)(h * 0.35f) + 6;
    int16_t lidW = w + 30;
    lv_obj_set_size(lid.top, lidW, lidH);
    lv_obj_set_pos(lid.top, cx - lidW / 2, cy - h / 2 - lidH / 2);
    lv_obj_set_style_radius(lid.top, lidH / 3, 0);
    int32_t angleDeg10 = (int32_t)(cfg.slopeTop * 250.0f);
    lv_obj_set_style_transform_rotation(lid.top, angleDeg10, 0);
    lv_obj_set_style_transform_pivot_x(lid.top, lidW / 2, 0);
    lv_obj_set_style_transform_pivot_y(lid.top, lidH, 0);
    lv_obj_clear_flag(lid.top, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(lid.top, LV_OBJ_FLAG_HIDDEN);
  }

  if (cfg.slopeBottom != 0.0f) {
    int16_t lidH = (int16_t)(h * 0.35f) + 6;
    int16_t lidW = w + 30;
    lv_obj_set_size(lid.bot, lidW, lidH);
    lv_obj_set_pos(lid.bot, cx - lidW / 2, cy + h / 2 - lidH / 2);
    lv_obj_set_style_radius(lid.bot, lidH / 3, 0);
    int32_t angleDeg10 = (int32_t)(cfg.slopeBottom * 250.0f);
    lv_obj_set_style_transform_rotation(lid.bot, angleDeg10, 0);
    lv_obj_set_style_transform_pivot_x(lid.bot, lidW / 2, 0);
    lv_obj_set_style_transform_pivot_y(lid.bot, 0, 0);
    lv_obj_clear_flag(lid.bot, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(lid.bot, LV_OBJ_FLAG_HIDDEN);
  }
}

void applyMouth(float mouthState) {
  if (!s_mouth) return;

  if (mouthState < -0.05f) {
    float openness = -mouthState;
    int16_t w = (int16_t)(30 + openness * 30);
    int16_t h = (int16_t)(8 + openness * 25);
    lv_obj_set_pos(s_mouth, MOUTH_CX - w / 2, MOUTH_CY - h / 2);
    lv_obj_set_size(s_mouth, w, h);
    lv_obj_set_style_radius(s_mouth, h / 2, 0);
    lv_obj_clear_flag(s_mouth, LV_OBJ_FLAG_HIDDEN);
  } else if (mouthState > 0.05f) {
    float smile = mouthState;
    int16_t w = (int16_t)(40 + smile * 30);
    int16_t h = (int16_t)(6 + smile * 8);
    lv_obj_set_pos(s_mouth, MOUTH_CX - w / 2, MOUTH_CY - h / 2);
    lv_obj_set_size(s_mouth, w, h);
    lv_obj_set_style_radius(s_mouth, h / 2, 0);
    lv_obj_clear_flag(s_mouth, LV_OBJ_FLAG_HIDDEN);
  } else {
    int16_t w = 25, h = 4;
    lv_obj_set_pos(s_mouth, MOUTH_CX - w / 2, MOUTH_CY - h / 2);
    lv_obj_set_size(s_mouth, w, h);
    lv_obj_set_style_radius(s_mouth, 2, 0);
    lv_obj_clear_flag(s_mouth, LV_OBJ_FLAG_HIDDEN);
  }
}

} // namespace

namespace FaceRenderer {

void init() {
  lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);

  initEye(s_leftEye);
  initEye(s_rightEye);
  s_leftLid.top = makeLid(); s_leftLid.bot = makeLid();
  s_rightLid.top = makeLid(); s_rightLid.bot = makeLid();
  initMouth();

  FacePreset normal = FacePresets::getPreset(FaceExpression::Normal);
  applyEye(s_leftEye, normal.left, LEFT_EYE_CX, EYE_CY, 0, 0);
  applyEye(s_rightEye, normal.right, RIGHT_EYE_CX, EYE_CY, 0, 0);
  applyMouth(0.0f);
}

void render(const EyeConfig& left, const EyeConfig& right, float lookX, float lookY, float mouthState) {
  if (!s_leftEye || !s_rightEye) return;
  applyEye(s_leftEye, left, LEFT_EYE_CX, EYE_CY, lookX, lookY);
  applyEye(s_rightEye, right, RIGHT_EYE_CX, EYE_CY, lookX, lookY);
  applyLid(s_leftLid, left, LEFT_EYE_CX, EYE_CY, lookX, lookY);
  applyLid(s_rightLid, right, RIGHT_EYE_CX, EYE_CY, lookX, lookY);
  applyMouth(mouthState);
}

} // namespace FaceRenderer
