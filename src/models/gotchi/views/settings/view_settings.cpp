#include "view_settings.h"
#include "../../config/config.h"
#include "../../config/gotchi_theme.h"
#include "common/managers/ble/ble_manager.h"
#include <lvgl.h>
#include <Arduino.h>
#include <display/Arduino_CO5300.h>
#include "common/managers/wifi/wifi_manager.h"

extern Arduino_GFX* getGotchiGfx();

namespace {

lv_obj_t* s_screen = nullptr;

// BLE
lv_obj_t* s_bleBtn = nullptr;
lv_obj_t* s_bleSym = nullptr;
lv_obj_t* s_bleLabel = nullptr;
bool s_bleOn = false;

// WiFi indicator
lv_obj_t* s_wifiIcon = nullptr;
bool s_wifiConnected = false;

// Brightness
lv_obj_t* s_brightSlider = nullptr;
lv_obj_t* s_brightLabel = nullptr;
lv_obj_t* s_brightIconL = nullptr;
lv_obj_t* s_brightIconR = nullptr;
uint8_t s_brightness = 200;

constexpr int16_t CX = GOTCHI_LCD_WIDTH / 2;
constexpr int16_t CY = GOTCHI_LCD_HEIGHT / 2;

void applyBrightness(uint8_t val) {
  s_brightness = val;
  Arduino_CO5300* disp = (Arduino_CO5300*)getGotchiGfx();
  if (disp) disp->setBrightness(val);
}

lv_color_t getEyeColor() {
  uint16_t tc = GotchiTheme::getColors().eye;
  return lv_color_make(
    ((tc >> 11) & 0x1F) << 3, ((tc >> 5) & 0x3F) << 2, (tc & 0x1F) << 3);
}

void updateBleIcon(lv_color_t eyeCol) {
  if (s_bleSym) {
    lv_obj_set_style_text_color(s_bleSym, s_bleOn ? eyeCol : lv_color_hex(0x555577), 0);
  }
  if (s_bleBtn) {
    lv_obj_set_style_border_color(s_bleBtn, s_bleOn ? eyeCol : lv_color_hex(0x333355), 0);
  }
  if (s_bleLabel) {
    lv_label_set_text(s_bleLabel, s_bleOn ? "BLE ON" : "BLE OFF");
    lv_obj_set_style_text_color(s_bleLabel, s_bleOn ? eyeCol : lv_color_hex(0x555577), 0);
  }
}

void updateWifiIcon() {
  if (!s_wifiIcon) return;
  bool connected = WiFiManager::isConnected();
  s_wifiConnected = connected;
  // Vert si connecte, gris si non
  lv_color_t col = connected
    ? lv_color_hex(0x22C55E)
    : lv_color_hex(0x444466);
  lv_obj_set_style_text_color(s_wifiIcon, col, 0);
}

// Callback BLE icon tap
void onBleTap(lv_event_t* e) {
  Serial.println("[SETTINGS] BLE tap");
  s_bleOn = !s_bleOn;
  if (s_bleOn) {
    BLEManager::startAdvertising();
  } else {
    BLEManager::stopAdvertising();
  }
  updateBleIcon(getEyeColor());
}

// Callback slider brightness
void onBrightSlider(lv_event_t* e) {
  lv_obj_t* sl = (lv_obj_t*)lv_event_get_target(e);
  int16_t val = lv_slider_get_value(sl);
  uint8_t hw = (uint8_t)(val * 255 / 100);
  applyBrightness(hw);
  if (s_brightLabel) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", val);
    lv_label_set_text(s_brightLabel, buf);
  }
}

void createUI() {
  s_screen = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
  lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

  uint16_t tc = GotchiTheme::getColors().eye;
  lv_color_t eyeCol = lv_color_make(
    ((tc >> 11) & 0x1F) << 3,
    ((tc >> 5) & 0x3F) << 2,
    (tc & 0x1F) << 3
  );

  // ============================
  // WiFi — indicateur en haut (non cliquable)
  // ============================
  // Conteneur avec scale pour agrandir visuellement (montserrat_24+ non compile dans la lib)
  s_wifiIcon = lv_label_create(s_screen);
  lv_label_set_text(s_wifiIcon, LV_SYMBOL_WIFI);
  lv_obj_set_style_text_font(s_wifiIcon, &lv_font_montserrat_14, 0);
  // Scale x2 (256 = 1x, 512 = 2x)
  lv_obj_set_style_transform_scale(s_wifiIcon, 512, 0);
  lv_obj_align(s_wifiIcon, LV_ALIGN_TOP_MID, 0, 70);
  updateWifiIcon();

  // ============================
  // BLE — bouton circulaire cliquable (grosse zone)
  // ============================
  // Note: ne pas utiliser BLEManager::isAvailable() (ca verifie l'init, pas l'advertising)
  s_bleOn = false;

  // Bouton circulaire 120x120 — large zone tap visible
  s_bleBtn = lv_button_create(s_screen);
  lv_obj_set_size(s_bleBtn, 120, 120);
  lv_obj_align(s_bleBtn, LV_ALIGN_CENTER, 0, -70);
  lv_obj_set_style_bg_color(s_bleBtn, lv_color_hex(0x1A1A2E), 0);
  lv_obj_set_style_bg_opa(s_bleBtn, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(s_bleBtn, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(s_bleBtn, 3, 0);
  lv_obj_set_style_border_color(s_bleBtn, lv_color_hex(0x333355), 0);
  lv_obj_set_style_shadow_width(s_bleBtn, 0, 0);
  lv_obj_add_event_cb(s_bleBtn, onBleTap, LV_EVENT_CLICKED, nullptr);

  // Label icone BLE a l'interieur du bouton (scale x3 pour la taille)
  lv_obj_t* bleSym = lv_label_create(s_bleBtn);
  lv_label_set_text(bleSym, LV_SYMBOL_BLUETOOTH);
  lv_obj_set_style_text_font(bleSym, &lv_font_montserrat_14, 0);
  lv_obj_set_style_transform_scale(bleSym, 768, 0);  // 3x
  lv_obj_center(bleSym);
  // Reference utilisee par updateBleIcon pour changer la couleur du symbole
  s_bleSym = bleSym;

  // Label texte sous le bouton
  s_bleLabel = lv_label_create(s_screen);
  lv_label_set_text(s_bleLabel, "BLE OFF");
  lv_obj_set_style_text_color(s_bleLabel, lv_color_hex(0x555577), 0);
  lv_obj_set_style_text_font(s_bleLabel, &lv_font_montserrat_14, 0);
  lv_obj_align(s_bleLabel, LV_ALIGN_CENTER, 0, 5);

  updateBleIcon(eyeCol);

  // ============================
  // Luminosite — slider horizontal
  // ============================

  // Icone soleil petit (gauche)
  s_brightIconL = lv_label_create(s_screen);
  lv_label_set_text(s_brightIconL, LV_SYMBOL_IMAGE);
  lv_obj_set_style_text_color(s_brightIconL, lv_color_hex(0x444466), 0);
  lv_obj_set_style_text_font(s_brightIconL, &lv_font_montserrat_14, 0);
  lv_obj_align(s_brightIconL, LV_ALIGN_CENTER, -100, 80);

  // Slider
  s_brightSlider = lv_slider_create(s_screen);
  lv_obj_set_size(s_brightSlider, 160, 8);
  lv_obj_align(s_brightSlider, LV_ALIGN_CENTER, 0, 80);
  lv_slider_set_range(s_brightSlider, 5, 100);
  lv_slider_set_value(s_brightSlider, (int16_t)(s_brightness * 100 / 255), LV_ANIM_OFF);
  // Track
  lv_obj_set_style_bg_color(s_brightSlider, lv_color_hex(0x222240), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(s_brightSlider, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(s_brightSlider, 4, LV_PART_MAIN);
  // Indicator
  lv_obj_set_style_bg_color(s_brightSlider, eyeCol, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(s_brightSlider, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(s_brightSlider, 4, LV_PART_INDICATOR);
  // Knob
  lv_obj_set_style_bg_color(s_brightSlider, eyeCol, LV_PART_KNOB);
  lv_obj_set_style_pad_all(s_brightSlider, 6, LV_PART_KNOB);
  lv_obj_add_event_cb(s_brightSlider, onBrightSlider, LV_EVENT_VALUE_CHANGED, nullptr);

  // Icone soleil grand (droite)
  s_brightIconR = lv_label_create(s_screen);
  lv_label_set_text(s_brightIconR, LV_SYMBOL_IMAGE);
  lv_obj_set_style_text_color(s_brightIconR, lv_color_hex(0x888899), 0);
  lv_obj_set_style_text_font(s_brightIconR, &lv_font_montserrat_14, 0);
  lv_obj_align(s_brightIconR, LV_ALIGN_CENTER, 100, 80);

  // Pourcentage sous le slider
  s_brightLabel = lv_label_create(s_screen);
  lv_obj_set_style_text_color(s_brightLabel, lv_color_hex(0x666688), 0);
  lv_obj_set_style_text_font(s_brightLabel, &lv_font_montserrat_14, 0);
  lv_obj_align(s_brightLabel, LV_ALIGN_CENTER, 0, 105);
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", (int)(s_brightness * 100 / 255));
  lv_label_set_text(s_brightLabel, buf);

  // === Page dots (3 pages, settings = page 2) ===
  for (int i = 0; i < 3; i++) {
    lv_obj_t* pd = lv_obj_create(s_screen);
    lv_obj_set_size(pd, 14, 4);
    lv_obj_set_pos(pd, CX - 27 + i * 18, 450);
    lv_obj_set_style_bg_color(pd, i == 2 ? eyeCol : lv_color_hex(0x2A2A40), 0);
    lv_obj_set_style_bg_opa(pd, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(pd, 2, 0);
    lv_obj_set_style_border_width(pd, 0, 0);
  }
}

} // namespace

static void settingsInit() { s_screen = nullptr; }

static void settingsUpdate(uint32_t dtMs, Arduino_GFX* gfx) {
  // Refresh WiFi indicator toutes les ~1s
  static uint32_t lastWifiCheck = 0;
  uint32_t now = millis();
  if (now - lastWifiCheck >= 1000) {
    lastWifiCheck = now;
    bool prev = s_wifiConnected;
    if (WiFiManager::isConnected() != prev) {
      updateWifiIcon();
    }
  }
}

static void settingsOnEnter(Arduino_GFX* gfx) {
  if (!s_screen) createUI();
  // Refresh icones a l'entree
  updateBleIcon(getEyeColor());
  updateWifiIcon();
  lv_screen_load(s_screen);
  lv_obj_invalidate(s_screen);
}

static void settingsOnExit(Arduino_GFX* gfx) {
  if (gfx) gfx->fillScreen(0x0000);
}

const View VIEW_SETTINGS = {
  "settings", settingsInit, settingsUpdate, settingsOnEnter, settingsOnExit, true
};
