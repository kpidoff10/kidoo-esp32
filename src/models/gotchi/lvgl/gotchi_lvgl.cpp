#include "gotchi_lvgl.h"

#include "../config/config.h"
#include "../face/face_engine.h"
#include "../face/overlay/face_overlay_layer.h"
#include "../face/behavior/behavior_engine.h"
#include "../face/behavior/dirt_overlay.h"
#include "../face/gotchi_haptic.h"
#include "../imu/gotchi_imu.h"
#include "../battery/gotchi_battery.h"
#include "../config/gotchi_theme.h"
#include "../views/view_manager.h"

#include <cstdint>
#include <cmath>
#include <cstring>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include <lvgl.h>

#include "touch/TouchDrvCST92xx.h"
#include "arduino_co5300_swrot90.h"

// GFX exposé pour face_renderer (dessin direct)
static Arduino_GFX *s_gfx = nullptr;
Arduino_GFX* getGotchiGfx() { return s_gfx; }

namespace {

Arduino_ESP32QSPI *s_bus = nullptr;
TouchDrvCST92xx s_touch;
bool s_touch_ok = false;
bool s_lvgl_ok = false;

static constexpr uint8_t kTouchI2cAddr = CST92XX_SLAVE_ADDRESS;

// ============================================
// LVGL 9 : Tick callback (remplace esp_timer + lv_tick_inc)
// ============================================
static uint32_t millis_cb(void) {
  return millis();
}

// Rounder : aligne les zones de redraw sur pixels pairs (requis par QSPI CO5300)
static void rounder_event_cb(lv_event_t *e) {
  lv_area_t *area = (lv_area_t *)lv_event_get_param(e);
  if (area->x1 & 1) area->x1--;
  if (area->y1 & 1) area->y1--;
  if (!(area->x2 & 1)) area->x2++;
  if (!(area->y2 & 1)) area->y2++;
}

// ============================================
// LVGL 9 : Flush callback
// ============================================
void disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  if (!s_gfx) {
    lv_display_flush_ready(disp);
    return;
  }
  const uint32_t w = lv_area_get_width(area);
  const uint32_t h = lv_area_get_height(area);
  s_gfx->draw16bitRGBBitmap(area->x1, area->y1, reinterpret_cast<uint16_t *>(px_map), w, h);
  lv_display_flush_ready(disp);
}

// ============================================
// Touch : conversion physique -> logique apres rotation 90° CW de l'ecran
// Le capteur CST9217 ne tourne pas avec l'ecran, on remappe les coords.
// Inverse de la rotation logique->physique : (lx, ly) = (py, W-1-px)
// ============================================
static inline void rotateTouchLogical(uint16_t px, uint16_t py, int16_t &lx, int16_t &ly) {
  lx = static_cast<int16_t>(py);
  ly = static_cast<int16_t>((GOTCHI_LCD_WIDTH - 1) - px);
}

// ============================================
// LVGL 9 : Touch read callback
// ============================================
void touchpad_read(lv_indev_t *drv, lv_indev_data_t *data) {
  if (!s_touch_ok) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }
  const TouchPoints &tp = s_touch.getTouchPoints();
  if (s_touch.isPressed() && tp.hasPoints()) {
    const auto &pt = tp.getPoint(0);
    int16_t lx, ly;
    rotateTouchLogical(pt.x, pt.y, lx, ly);
    data->point.x = static_cast<int32_t>(lx);
    data->point.y = static_cast<int32_t>(ly);
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

} // namespace

namespace GotchiLvgl {

bool init() {
  if (s_lvgl_ok) return true;

  Serial.println("[GOTCHI_LVGL] Initialisation GFX...");

  s_bus = new Arduino_ESP32QSPI(
      GOTCHI_LCD_CS, GOTCHI_LCD_SCLK, GOTCHI_LCD_SDIO0, GOTCHI_LCD_SDIO1, GOTCHI_LCD_SDIO2,
      GOTCHI_LCD_SDIO3);
  s_gfx = new Arduino_CO5300_SwRot90(s_bus, GOTCHI_LCD_RESET, 0, false, GOTCHI_LCD_WIDTH, GOTCHI_LCD_HEIGHT, 6, 0, 0, 0);

  if (!s_gfx->begin()) {
    Serial.println("[GOTCHI_LVGL] ERREUR: gfx->begin() échouée");
    return false;
  }

  Serial.println("[GOTCHI_LVGL] GFX OK, initialisation LVGL 9...");

  lv_init();

  // Tick source
  lv_tick_set_cb(millis_cb);

  // Buffers (taille en bytes pour LVGL 9)
  const uint32_t buf_pixels = (GOTCHI_LCD_WIDTH * GOTCHI_LCD_HEIGHT) / 10;
  const size_t buf_bytes = buf_pixels * sizeof(uint16_t); // RGB565 = 2 bytes/pixel

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

  // Display (LVGL 9 API)
  lv_display_t *disp = lv_display_create(GOTCHI_LCD_WIDTH, GOTCHI_LCD_HEIGHT);
  lv_display_set_flush_cb(disp, disp_flush);
  lv_display_set_buffers(disp, buf1, buf2, buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_add_event_cb(disp, rounder_event_cb, LV_EVENT_INVALIDATE_AREA, nullptr);

  // Touch (LVGL 9 API)
  s_touch.setPins(GOTCHI_TP_RESET, GOTCHI_TP_INT);
  s_touch.setMaxCoordinates(GOTCHI_LCD_WIDTH - 1, GOTCHI_LCD_HEIGHT - 1);
  s_touch.setMirrorXY(true, true);
  s_touch_ok = s_touch.begin(Wire, kTouchI2cAddr, IIC_SDA, IIC_SCL);
  if (!s_touch_ok && Serial) {
    Serial.println("[GOTCHI_LVGL] Touch non initialisé (UI sans pointer)");
  }

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touchpad_read);

  // Initialiser le Face Engine + Behavior Engine
  GotchiTheme::loadFromConfig();
  FaceEngine::init();
  BehaviorEngine::init();
  GotchiImu::init();
  GotchiBattery::init();
  ViewManager::init(s_gfx);

  s_lvgl_ok = true;
  Serial.println("[GOTCHI_LVGL] Initialisation LVGL 9 complète");
  return true;
}

// --- Touch detection (tap + caress) ---
bool     s_wasTouched = false;
uint32_t s_touchDownAt = 0;
uint32_t s_lastTapAt = 0;
int16_t  s_touchStartX = 0;
int16_t  s_touchStartY = 0;
int16_t  s_touchLastX = 0;
int16_t  s_touchLastY = 0;
int16_t  s_touchPrevX = 0;     // position au dernier sample (pour delta incremental)
int16_t  s_touchPrevY = 0;
uint32_t s_pathLen = 0;        // chemin total parcouru depuis touchDown (somme des |delta|)
uint32_t s_lastMoveAt = 0;     // dernier moment ou le doigt a vraiment bouge (>1px)
uint32_t s_lastPetAt = 0;
bool     s_isPetting = false;
constexpr uint32_t TAP_MAX_DURATION = 700;   // tap permissif (pouce un peu lent)
constexpr uint32_t TAP_DEBOUNCE     = 300;
// Caresse = vrai frottement continu, pas un long press immobile.
constexpr uint32_t PET_MIN_HOLD     = 400;   // ms minimum de contact
constexpr uint32_t PET_PATH_THRESH  = 80;    // px de chemin cumule (path length, pas dist start->now)
constexpr int16_t  PET_MOVE_NOISE   = 2;     // px : delta plus petit = bruit capteur, ignore
constexpr uint32_t PET_RECENT_MOVE  = 200;   // ms : si pas bouge depuis ce delai, doigt arrete = pas une caresse
constexpr uint32_t PET_INTERVAL     = 600;   // ms entre chaque event pet
constexpr uint32_t SWIPE_MAX_DURATION = 500; // ms max pour un swipe
constexpr int16_t  SWIPE_MIN_DIST    = 40;   // pixels minimum pour un swipe
bool     s_stablePressed = false;
uint32_t s_releaseStart = 0;
constexpr uint32_t RELEASE_CONFIRM = 250;
bool     s_pageSwipeConsumed = false;  // swipe page deja declenche ce touch
constexpr int16_t PAGE_SWIPE_THRESH = 60;  // px pour declencher un changement de page

void update() {
  if (s_lvgl_ok) {
    static uint32_t lastMs = 0;
    uint32_t now = millis();
    uint32_t dt = lastMs ? (now - lastMs) : 10;
    lastMs = now;

    // Touch detection
    if (s_touch_ok) {
      bool rawPressed = s_touch.isPressed();
      const TouchPoints &tp = s_touch.getTouchPoints();

      // Debounce release (capteur CST92xx bruyant)
      if (rawPressed) {
        s_stablePressed = true;
        s_releaseStart = 0;
      } else if (s_stablePressed) {
        if (s_releaseStart == 0) s_releaseStart = now;
        if ((now - s_releaseStart) >= RELEASE_CONFIRM) {
          s_stablePressed = false;
        }
      }
      bool pressed = s_stablePressed;

      if (pressed && !s_wasTouched) {
        // Doigt pose
        s_touchDownAt = now;
        s_isPetting = false;
        // En mode wash/brush, bloquer le swipe dès le début du touch
        s_pageSwipeConsumed = DirtOverlay::isWashMode() || DirtOverlay::isBrushMode();
        s_pathLen = 0;
        s_lastMoveAt = now;
        if (tp.hasPoints()) {
          const auto &pt = tp.getPoint(0);
          int16_t lx, ly;
          rotateTouchLogical(pt.x, pt.y, lx, ly);
          s_touchStartX = lx;
          s_touchStartY = ly;
          s_touchLastX  = lx;
          s_touchLastY  = ly;
          s_touchPrevX  = lx;
          s_touchPrevY  = ly;
        }
        // Finger down (seulement sur la view face en mode user action)
        if (ViewManager::isFaceView()) {
          bool isUserAction = (strcmp(BehaviorEngine::getCurrentBehavior(), "play") == 0);
          if (isUserAction) {
            BehaviorEngine::onFingerDown((float)s_touchStartX, (float)s_touchStartY);
          }
        }
      } else if (pressed && s_wasTouched) {
        if (tp.hasPoints()) {
          const auto &pt = tp.getPoint(0);
          int16_t lx, ly;
          rotateTouchLogical(pt.x, pt.y, lx, ly);

          // Delta incremental depuis le sample precedent (pas depuis le start).
          // Filtre le bruit capteur (<2 px) pour pas accumuler du chemin sur un doigt immobile.
          int16_t stepDx = lx - s_touchPrevX;
          int16_t stepDy = ly - s_touchPrevY;
          int16_t aStepDx = stepDx > 0 ? stepDx : -stepDx;
          int16_t aStepDy = stepDy > 0 ? stepDy : -stepDy;
          if (aStepDx > PET_MOVE_NOISE || aStepDy > PET_MOVE_NOISE) {
            s_pathLen += (uint32_t)(aStepDx + aStepDy);
            s_lastMoveAt = now;
            s_touchPrevX = lx;
            s_touchPrevY = ly;
          }

          s_touchLastX = lx;
          s_touchLastY = ly;

          // Detect swipe page LIVE (pendant le mouvement, pas au lacher)
          // Bloquer le swipe en mode wash (frottement pour nettoyer)
          if (!s_pageSwipeConsumed && !s_isPetting && !DirtOverlay::isWashMode() && !DirtOverlay::isBrushMode()) {
            int16_t swDx = lx - s_touchStartX;
            int16_t swDy = ly - s_touchStartY;
            int16_t aSwDx = swDx > 0 ? swDx : -swDx;
            int16_t aSwDy = swDy > 0 ? swDy : -swDy;
            if (aSwDx > PAGE_SWIPE_THRESH && aSwDx > aSwDy * 2) {
              if (ViewManager::handleSwipe(swDx, swDy, s_gfx)) {
                s_pageSwipeConsumed = true;
              }
            }
          }

          // Mode wash ou brush (prioritaire)
          if (DirtOverlay::isWashMode()) {
            DirtOverlay::onFingerMove((float)lx, (float)ly);
          } else if (DirtOverlay::isBrushMode()) {
            DirtOverlay::onBrushFingerMove((float)lx, (float)ly);
          } else
          // Si le swipe page a ete consomme, ne rien faire d'autre
          if (s_pageSwipeConsumed) {
            // Skip tout le reste du touch move
          } else
          // Touch move : seulement sur la view face
          if (ViewManager::isFaceView()) {
            bool isUserAction = (strcmp(BehaviorEngine::getCurrentBehavior(), "play") == 0);
            if (isUserAction) {
              BehaviorEngine::onFingerMove((float)lx, (float)ly);
            } else {
              // Caresse = vrai frottement continu :
              //   - assez de temps de contact (> PET_MIN_HOLD)
              //   - chemin cumule significatif (path length, ignore le bruit capteur)
              //   - mouvement RECENT (le doigt est encore en train de bouger,
              //     pas un long press qui s'est immobilise)
              uint32_t held = now - s_touchDownAt;
              bool moving = (now - s_lastMoveAt) < PET_RECENT_MOVE;
              if (held > PET_MIN_HOLD && s_pathLen > PET_PATH_THRESH && moving) {
                s_isPetting = true;
                float normX = ((float)lx - (float)(GOTCHI_LCD_WIDTH / 2)) / (float)(GOTCHI_LCD_WIDTH / 2);
                float normY = ((float)ly - (float)(GOTCHI_LCD_HEIGHT / 2)) / (float)(GOTCHI_LCD_HEIGHT / 2);
                if (normX > 1.0f) normX = 1.0f;
                if (normX < -1.0f) normX = -1.0f;
                if (normY > 1.0f) normY = 1.0f;
                if (normY < -1.0f) normY = -1.0f;
                FaceEngine::lookAtForced(normX, normY * 0.6f);
                if ((now - s_lastPetAt) > PET_INTERVAL) {
                  s_lastPetAt = now;
                  BehaviorEngine::onPet();
                }
              }
            }
          }
        }
      } else if (!pressed && s_wasTouched) {
        // Doigt leve
        uint32_t duration = now - s_touchDownAt;
        int16_t totalDx = s_touchLastX - s_touchStartX;
        int16_t totalDy = s_touchLastY - s_touchStartY;
        int16_t totalDist = (totalDx > 0 ? totalDx : -totalDx) + (totalDy > 0 ? totalDy : -totalDy);
        // Caresse uniquement si elle a été détectée pendant CE press.
        // (pas de fenêtre temporelle qui mangeait le tap suivant)
        bool wasPetting = s_isPetting;

        // 1) Swipe → si deja consomme par page swipe live, skip
        if (s_pageSwipeConsumed) {
          // Page deja changee pendant le mouvement
        } else if (!wasPetting && !DirtOverlay::isWashMode() && !DirtOverlay::isBrushMode() && totalDist > SWIPE_MIN_DIST && duration < SWIPE_MAX_DURATION) {
          if (ViewManager::handleSwipe(totalDx, totalDy, s_gfx)) {
            // Page changee
          } else if (ViewManager::isFaceView()) {
            // Swipe non consomme + sur face → onSwipe behavior
            float mag = sqrtf((float)(totalDx * totalDx + totalDy * totalDy));
            BehaviorEngine::onSwipe((float)s_touchStartX, (float)s_touchStartY,
                                    (float)totalDx / mag, (float)totalDy / mag);
          }
        } else if (ViewManager::isFaceView()) {
          // Sur la view face : finger up ou tap
          bool isUserAction = (strcmp(BehaviorEngine::getCurrentBehavior(), "play") == 0);
          if (isUserAction) {
            float velScale = (duration > 0) ? 1000.0f / (float)duration : 0.0f;
            float fvx = (float)totalDx * velScale * 0.0003f;
            float fvy = (float)totalDy * velScale * 0.0003f;
            BehaviorEngine::onFingerUp((float)s_touchLastX, (float)s_touchLastY, fvx, fvy);
          } else if (!wasPetting && totalDist <= 20 && duration < TAP_MAX_DURATION && (now - s_lastTapAt) > TAP_DEBOUNCE) {
            s_lastTapAt = now;
            // Tap localise : reactions selon zone (yeux, bouche, chatouilles, front, cotes)
            BehaviorEngine::onTouchAt(s_touchLastX, s_touchLastY);
          }
        }
        s_isPetting = false;
      }
      s_wasTouched = pressed;
    }

    // ViewManager update + draw (dispatch a la view active)
    ViewManager::update(dt, s_gfx);

    // Haptique : auto-stop des pulsations non-bloquantes
    GotchiHaptic::update();

    vTaskDelay(1);
  }
}

void testDisplay() {
  if (!s_lvgl_ok) {
    Serial.println("[GOTCHI_LVGL] Écran non initialisé");
    return;
  }

  Serial.println("[GOTCHI_LVGL] Test écran: 5 couleurs (2s chacune)");

  const uint32_t colors[] = { 0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00, 0xFFFFFF };
  const char *names[] = {"ROUGE", "VERT", "BLEU", "JAUNE", "BLANC"};

  for (int i = 0; i < 5; i++) {
    lv_obj_clean(lv_screen_active());
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(colors[i]), 0);
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);

    lv_color_t text_color = (i == 1 || i == 2) ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x000000);
    lv_obj_t *label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, names[i]);
    lv_obj_set_style_text_color(label, text_color, 0);
    lv_obj_center(label);

    lv_timer_handler();
    Serial.printf("[GOTCHI_LVGL] Test %s\n", names[i]);
    delay(2000);
  }

  // Revenir au Face Engine
  lv_obj_clean(lv_screen_active());
  FaceOverlayLayer::init();
  FaceEngine::init();
  lv_timer_handler();
  Serial.println("[GOTCHI_LVGL] Test terminé");
}

} // namespace GotchiLvgl
