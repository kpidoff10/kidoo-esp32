#include "gotchi_lvgl.h"

#include "../config/config.h"

#include <cstdint>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <lvgl.h>

#include "touch/TouchDrvCST92xx.h"

namespace {

Arduino_ESP32QSPI *s_bus = nullptr;
Arduino_GFX *s_gfx = nullptr;
TouchDrvCST92xx s_touch;
bool s_touch_ok = false;
bool s_lvgl_ok = false;

esp_timer_handle_t s_lv_tick_timer = nullptr;

static constexpr uint8_t kTouchI2cAddr = CST92XX_SLAVE_ADDRESS;

void lv_tick_cb(void * /*arg*/) { lv_tick_inc(2); }

void rounder_cb(lv_disp_drv_t * /*disp_drv*/, lv_area_t *area) {
  if (area->x1 & 1) {
    area->x1--;
  }
  if (area->y1 & 1) {
    area->y1--;
  }
  if (!(area->x2 & 1)) {
    area->x2++;
  }
  if (!(area->y2 & 1)) {
    area->y2++;
  }
}

void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
  if (!s_gfx) {
    lv_disp_flush_ready(disp_drv);
    return;
  }
  const uint32_t w = lv_area_get_width(area);
  const uint32_t h = lv_area_get_height(area);
#if (LV_COLOR_16_SWAP != 0)
  s_gfx->draw16bitBeRGBBitmap(area->x1, area->y1, reinterpret_cast<uint16_t *>(color_p), w, h);
#else
  s_gfx->draw16bitRGBBitmap(area->x1, area->y1, reinterpret_cast<uint16_t *>(color_p), w, h);
#endif
  lv_disp_flush_ready(disp_drv);
}

void touchpad_read(lv_indev_drv_t * /*drv*/, lv_indev_data_t *data) {
  if (!s_touch_ok) {
    data->state = LV_INDEV_STATE_REL;
    return;
  }
  const TouchPoints &tp = s_touch.getTouchPoints();
  if (s_touch.isPressed() && tp.hasPoints()) {
    const TouchPoint &pt = tp.getPoint(0);
    data->point.x = static_cast<lv_coord_t>(pt.x);
    data->point.y = static_cast<lv_coord_t>(pt.y);
    data->state = LV_INDEV_STATE_PR;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

} // namespace

namespace GotchiLvgl {

bool init() {
  if (s_lvgl_ok) {
    return true;
  }

  Serial.println("[GOTCHI_LVGL] Initialisation GFX...");

  s_bus = new Arduino_ESP32QSPI(
      GOTCHI_LCD_CS, GOTCHI_LCD_SCLK, GOTCHI_LCD_SDIO0, GOTCHI_LCD_SDIO1, GOTCHI_LCD_SDIO2,
      GOTCHI_LCD_SDIO3);
  s_gfx = new Arduino_CO5300(s_bus, GOTCHI_LCD_RESET, 0, false, GOTCHI_LCD_WIDTH, GOTCHI_LCD_HEIGHT, 6, 0, 0, 0);

  if (!s_gfx->begin()) {
    Serial.println("[GOTCHI_LVGL] ERREUR: gfx->begin() échouée");
    return false;
  }

  Serial.println("[GOTCHI_LVGL] GFX OK, initialisation LVGL...");

  lv_init();

  const uint32_t buf_pixels = (GOTCHI_LCD_WIDTH * GOTCHI_LCD_HEIGHT) / 10;
  const size_t buf_bytes = buf_pixels * sizeof(lv_color_t);

  // Essayer d'abord PSRAM, sinon DMA, sinon normal
  void *buf1 = heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM);
  if (!buf1) buf1 = heap_caps_malloc(buf_bytes, MALLOC_CAP_DMA);
  if (!buf1) buf1 = heap_caps_malloc(buf_bytes, MALLOC_CAP_DEFAULT);

  void *buf2 = heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM);
  if (!buf2) buf2 = heap_caps_malloc(buf_bytes, MALLOC_CAP_DMA);
  if (!buf2) buf2 = heap_caps_malloc(buf_bytes, MALLOC_CAP_DEFAULT);

  if (!buf1 || !buf2) {
    Serial.printf("[GOTCHI_LVGL] ERREUR: Allocation mémoire échouée (buf1=%p, buf2=%p)\n", buf1, buf2);
    if (buf1) heap_caps_free(buf1);
    if (buf2) heap_caps_free(buf2);
    return false;
  }

  Serial.printf("[GOTCHI_LVGL] Buffers LVGL alloués (%zu bytes chacun)\n", buf_bytes);

  static lv_disp_draw_buf_t draw_buf;
  lv_disp_draw_buf_init(&draw_buf, static_cast<lv_color_t *>(buf1), static_cast<lv_color_t *>(buf2),
                        buf_pixels);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = GOTCHI_LCD_WIDTH;
  disp_drv.ver_res = GOTCHI_LCD_HEIGHT;
  disp_drv.flush_cb = disp_flush;
  disp_drv.draw_buf = &draw_buf;
  disp_drv.rounder_cb = rounder_cb;
  lv_disp_drv_register(&disp_drv);

  s_touch.setPins(GOTCHI_TP_RESET, GOTCHI_TP_INT);
  s_touch.setMaxCoordinates(GOTCHI_LCD_WIDTH - 1, GOTCHI_LCD_HEIGHT - 1);
  s_touch.setMirrorXY(true, true);
  s_touch_ok = s_touch.begin(Wire, kTouchI2cAddr, IIC_SDA, IIC_SCL);
  if (!s_touch_ok && Serial) {
    Serial.println("[GOTCHI_LVGL] Touch non initialisé (UI sans pointer)");
  }

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = touchpad_read;
  lv_indev_drv_register(&indev_drv);

  esp_timer_create_args_t tick_args = {};
  tick_args.callback = &lv_tick_cb;
  tick_args.name = "lv_tick";
  if (esp_timer_create(&tick_args, &s_lv_tick_timer) == ESP_OK && s_lv_tick_timer) {
    esp_timer_start_periodic(s_lv_tick_timer, 2000);
  } else if (Serial) {
    Serial.println("[GOTCHI_LVGL] esp_timer LVGL non créé (utiliser millis() si besoin)");
  }

  // Écran de test Gotchi blanc/noir simple
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);

  lv_obj_t *label = lv_label_create(lv_scr_act());
  lv_label_set_text(label, "GOTCHI");
  lv_obj_set_style_text_color(label, lv_color_hex(0x000000), 0);
  lv_obj_center(label);

  s_lvgl_ok = true;
  Serial.println("[GOTCHI_LVGL] ✅ Initialisation complète - écran prêt");
  return true;
}

void update() {
  if (s_lvgl_ok) {
    lv_timer_handler();
  }
}

void testDisplay() {
  if (!s_lvgl_ok) {
    Serial.println("[GOTCHI_LVGL] Écran non initialisé");
    return;
  }

  Serial.println("[GOTCHI_LVGL] Test écran: 5 couleurs (2s chacune)");

  const lv_color_t colors[] = {
    lv_color_hex(0xFF0000),   // Rouge
    lv_color_hex(0x00FF00),   // Vert
    lv_color_hex(0x0000FF),   // Bleu
    lv_color_hex(0xFFFF00),   // Jaune
    lv_color_hex(0xFFFFFF)    // Blanc
  };
  const char *names[] = {"ROUGE", "VERT", "BLEU", "JAUNE", "BLANC"};

  for (int i = 0; i < 5; i++) {
    // Effacer l'écran
    lv_obj_clean(lv_scr_act());

    // Fond coloré
    lv_obj_set_style_bg_color(lv_scr_act(), colors[i], 0);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);

    // Texte blanc si fond foncé, noir sinon
    lv_color_t text_color = (i == 1 || i == 2) ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x000000);

    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, names[i]);
    lv_obj_set_style_text_color(label, text_color, 0);
    lv_obj_center(label);

    // Mise à jour LVGL
    lv_timer_handler();

    Serial.printf("[GOTCHI_LVGL] Test %s\n", names[i]);
    delay(2000);
  }

  // Revenir au texte Gotchi original
  lv_obj_clean(lv_scr_act());
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);

  lv_obj_t *label = lv_label_create(lv_scr_act());
  lv_label_set_text(label, "GOTCHI");
  lv_obj_set_style_text_color(label, lv_color_hex(0x000000), 0);
  lv_obj_center(label);

  lv_timer_handler();
  Serial.println("[GOTCHI_LVGL] ✅ Test terminé");
}

} // namespace GotchiLvgl
