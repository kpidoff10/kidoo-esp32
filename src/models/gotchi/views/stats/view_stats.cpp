#include "view_stats.h"
#include "../../config/config.h"
#include "../../config/gotchi_theme.h"
#include "../../face/behavior/behavior_engine.h"
#include "../../face/face_engine.h"
#include "../../face/face_renderer.h"
#include "../../battery/gotchi_battery.h"
#include <lvgl.h>
#include <Arduino.h>
#include <cmath>

namespace {

lv_obj_t* s_screen = nullptr;

constexpr int16_t CX = GOTCHI_LCD_WIDTH / 2;
constexpr int16_t CY = GOTCHI_LCD_HEIGHT / 2;
constexpr float DEG2RAD = 3.14159265f / 180.0f;

// Viewport du face engine dans le FB (ne flush que cette zone)
// Face engine scale 0.6 → yeux + bouche 60% de la taille normale
// Viewport adapte : couvre le visage scale sans toucher les arcs (rayon 215+)
// Tous les coins du viewport sont a < 150px du centre → bien a l'interieur
constexpr float FACE_SCALE = 0.6f;
constexpr int16_t VP_X = 86;
constexpr int16_t VP_Y = 40;
constexpr int16_t VP_W = 254;
constexpr int16_t VP_H = 135;

struct StatWidget {
  lv_obj_t* arc;
  lv_obj_t* iconContainer;
};

StatWidget s_stats[4];
float s_prevPcts[4] = {-1,-1,-1,-1};
lv_obj_t* s_batIcon = nullptr;
lv_obj_t* s_batPctLabel = nullptr;
int8_t s_prevBatPct = -2;  // force first update

float statToPct(float val) {
  if (val < 0) return 0;
  if (val > 100) return 1.0f;
  return val / 100.0f;
}

lv_color_t colorForPct(float pct) {
  if (pct >= 0.6f) return lv_color_hex(0x47E060);
  if (pct >= 0.3f) return lv_color_hex(0xFFA500);
  return lv_color_hex(0xFF2020);
}

// === Helpers pour dessiner des icones avec des objets LVGL ===

void tagColored(lv_obj_t* o) { lv_obj_set_user_data(o, (void*)1); }

lv_obj_t* mkCircle(lv_obj_t* p, int16_t x, int16_t y, int16_t d, lv_color_t c, bool colored = true) {
  lv_obj_t* o = lv_obj_create(p);
  lv_obj_set_size(o, d, d);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_style_bg_color(o, c, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_pad_all(o, 0, 0);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  if (colored) tagColored(o);
  return o;
}

lv_obj_t* mkRect(lv_obj_t* p, int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, lv_color_t c, bool colored = true) {
  lv_obj_t* o = lv_obj_create(p);
  lv_obj_set_size(o, w, h);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_style_bg_color(o, c, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(o, r, 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_pad_all(o, 0, 0);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  if (colored) tagColored(o);
  return o;
}

lv_obj_t* mkContainer(lv_obj_t* p, int16_t cx, int16_t cy, int16_t sz) {
  lv_obj_t* c = lv_obj_create(p);
  lv_obj_set_size(c, sz, sz);
  lv_obj_set_pos(c, cx - sz / 2, cy - sz / 2);
  lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(c, 0, 0);
  lv_obj_set_style_pad_all(c, 0, 0);
  lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  return c;
}

void updateIconColor(lv_obj_t* container, lv_color_t color) {
  uint32_t n = lv_obj_get_child_count(container);
  for (uint32_t i = 0; i < n; i++) {
    lv_obj_t* ch = lv_obj_get_child(container, i);
    if (lv_obj_get_user_data(ch) == (void*)1) {
      lv_obj_set_style_bg_color(ch, color, 0);
      lv_obj_set_style_text_color(ch, color, 0);  // pour les labels (Zzz)
    }
  }
}

// Zzz — energie/sommeil
lv_obj_t* makeZzz(lv_obj_t* parent, int16_t cx, int16_t cy, lv_color_t col) {
  lv_obj_t* c = mkContainer(parent, cx, cy, 30);
  lv_obj_t* lbl = lv_label_create(c);
  lv_label_set_text(lbl, "Zzz");
  lv_obj_set_style_text_color(lbl, col, 0);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl);
  tagColored(lbl);
  return c;
}

// Coeur — vie/sante
lv_obj_t* makeHeart(lv_obj_t* parent, int16_t cx, int16_t cy, lv_color_t col) {
  lv_obj_t* c = mkContainer(parent, cx, cy, 26);
  // Deux bosses du haut
  mkCircle(c, 2, 4, 12, col);
  mkCircle(c, 12, 4, 12, col);
  // Corps central
  mkRect(c, 4, 9, 18, 6, 0, col);
  // Pointe du bas : triangle approche avec des rectangles de plus en plus etroits
  mkRect(c, 6, 15, 14, 3, 0, col);
  mkRect(c, 8, 18, 10, 3, 0, col);
  mkRect(c, 10, 21, 6, 2, 1, col);
  mkRect(c, 12, 23, 2, 2, 1, col);
  return c;
}

// Gouttes de douche — proprete
lv_obj_t* makeShower(lv_obj_t* parent, int16_t cx, int16_t cy, lv_color_t col) {
  lv_obj_t* c = mkContainer(parent, cx, cy, 26);
  mkRect(c, 5, 2, 14, 5, 3, col);
  mkRect(c, 17, 2, 4, 9, 2, col);
  mkCircle(c, 6,  10, 4, col);
  mkCircle(c, 13, 11, 4, col);
  mkCircle(c, 8,  16, 4, col);
  mkCircle(c, 15, 18, 4, col);
  mkCircle(c, 10, 21, 4, col);
  return c;
}

// Cuisse de poulet 45° — nourriture
lv_obj_t* makeDrumstick(lv_obj_t* parent, int16_t cx, int16_t cy, lv_color_t col) {
  lv_obj_t* c = mkContainer(parent, cx, cy, 28);
  // Viande (haut-gauche)
  mkCircle(c, 0, 0, 15, col);
  // Os en diagonale (petits cercles)
  mkCircle(c, 13, 13, 4, col);
  mkCircle(c, 16, 16, 4, col);
  // Noeuds d'os (bas-droite)
  mkCircle(c, 18, 22, 5, col);
  mkCircle(c, 22, 18, 5, col);
  return c;
}

typedef lv_obj_t* (*IconMaker)(lv_obj_t*, int16_t, int16_t, lv_color_t);

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

  // === 4 arcs suivant le cercle de l'ecran ===
  // Rotation 45° : coupures aux points cardinaux (0°, 90°, 180°, 270°)
  constexpr int16_t ARC_SIZE = 450;
  constexpr int16_t ARC_W = 10;
  constexpr float ICON_R = 185.0f;  // proche des arcs (rayon 215-225)

  struct ArcDef {
    int16_t startAngle;
    int16_t endAngle;
    float iconAngle;
    IconMaker makeIcon;
  };

  ArcDef defs[4] = {
    { 280, 350, 315, makeZzz },        // ENERGIE — haut-droite
    {  10,  80,  45, makeHeart },      // VIE — bas-droite
    { 100, 170, 135, makeShower },     // PROPRETE — bas-gauche
    { 190, 260, 225, makeDrumstick },  // NOURRITURE — haut-gauche
  };

  for (int i = 0; i < 4; i++) {
    ArcDef& d = defs[i];

    // Arc
    s_stats[i].arc = lv_arc_create(s_screen);
    lv_obj_set_size(s_stats[i].arc, ARC_SIZE, ARC_SIZE);
    lv_obj_center(s_stats[i].arc);
    lv_arc_set_bg_angles(s_stats[i].arc, d.startAngle, d.endAngle);
    lv_arc_set_range(s_stats[i].arc, 0, 100);
    lv_arc_set_value(s_stats[i].arc, 75);
    lv_obj_remove_style(s_stats[i].arc, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(s_stats[i].arc, LV_OBJ_FLAG_CLICKABLE);
    // Track
    lv_obj_set_style_arc_color(s_stats[i].arc, lv_color_hex(0x151525), LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_stats[i].arc, ARC_W, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(s_stats[i].arc, true, LV_PART_MAIN);
    // Indicator
    lv_obj_set_style_arc_color(s_stats[i].arc, lv_color_hex(0x47E060), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_stats[i].arc, ARC_W, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(s_stats[i].arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_stats[i].arc, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_stats[i].arc, 0, 0);

    // Icone au milieu de l'arc, vers l'interieur
    float ax = CX + ICON_R * cosf(d.iconAngle * DEG2RAD);
    float ay = CY + ICON_R * sinf(d.iconAngle * DEG2RAD);
    s_stats[i].iconContainer = d.makeIcon(s_screen, (int16_t)ax, (int16_t)ay, lv_color_hex(0x47E060));
  }

  // Pas de mini-gotchi LVGL — le vrai face engine dessine par-dessus au centre

  // === Batterie en haut (entre les 2 arcs du haut, gap a 270°) ===
  if (GotchiBattery::isAvailable()) {
    // Icone batterie
    s_batIcon = lv_label_create(s_screen);
    lv_obj_set_style_text_color(s_batIcon, lv_color_hex(0x555577), 0);
    lv_obj_set_style_text_font(s_batIcon, &lv_font_montserrat_14, 0);
    lv_obj_align(s_batIcon, LV_ALIGN_TOP_MID, 0, 14);
    lv_label_set_text(s_batIcon, LV_SYMBOL_BATTERY_FULL);
    // Pourcentage en dessous
    s_batPctLabel = lv_label_create(s_screen);
    lv_obj_set_style_text_color(s_batPctLabel, lv_color_hex(0x555577), 0);
    lv_obj_set_style_text_font(s_batPctLabel, &lv_font_montserrat_14, 0);
    lv_obj_align(s_batPctLabel, LV_ALIGN_TOP_MID, 0, 32);
    lv_label_set_text(s_batPctLabel, "100%");
  }

  // === Page dots (3 pages : stats=0, face=1, settings=2) ===
  for (int i = 0; i < 3; i++) {
    lv_obj_t* pd = lv_obj_create(s_screen);
    lv_obj_set_size(pd, 14, 4);
    lv_obj_set_pos(pd, CX - 27 + i * 18, 450);
    lv_obj_set_style_bg_color(pd, i == 0 ? eyeCol : lv_color_hex(0x2A2A40), 0);
    lv_obj_set_style_bg_opa(pd, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(pd, 2, 0);
    lv_obj_set_style_border_width(pd, 0, 0);
  }
}

void updateWidgets(const BehaviorStats& stats) {
  float pcts[4] = {
    statToPct(stats.energy),    // ENERGIE
    statToPct(stats.health),    // VIE
    statToPct(stats.hygiene),   // PROPRETE
    statToPct(stats.hunger),    // NOURRITURE
  };

  for (int i = 0; i < 4; i++) {
    if (fabsf(pcts[i] - s_prevPcts[i]) < 0.02f) continue;

    lv_color_t col = colorForPct(pcts[i]);
    int val = (int)(pcts[i] * 100.0f);

    lv_arc_set_value(s_stats[i].arc, val);
    lv_obj_set_style_arc_color(s_stats[i].arc, col, LV_PART_INDICATOR);
    updateIconColor(s_stats[i].iconContainer, col);

    s_prevPcts[i] = pcts[i];
  }

  // Batterie
  if (s_batIcon) {
    int8_t pct = GotchiBattery::getPercent();
    if (pct != s_prevBatPct) {
      s_prevBatPct = pct;
      const char* icon = LV_SYMBOL_BATTERY_FULL;
      lv_color_t col = lv_color_hex(0x47E060);
      if (pct < 0) {
        icon = LV_SYMBOL_BATTERY_EMPTY;
        col = lv_color_hex(0x555577);
      } else if (pct <= 10) {
        icon = LV_SYMBOL_BATTERY_EMPTY;
        col = lv_color_hex(0xFF2020);
      } else if (pct <= 25) {
        icon = LV_SYMBOL_BATTERY_1;
        col = lv_color_hex(0xFFA500);
      } else if (pct <= 50) {
        icon = LV_SYMBOL_BATTERY_2;
        col = lv_color_hex(0xFFA500);
      } else if (pct <= 75) {
        icon = LV_SYMBOL_BATTERY_3;
        col = lv_color_hex(0x47E060);
      }
      // Icone (+ eclair si branche)
      if (GotchiBattery::isPluggedIn()) {
        char ibuf[16];
        snprintf(ibuf, sizeof(ibuf), "%s " LV_SYMBOL_CHARGE, icon);
        lv_label_set_text(s_batIcon, ibuf);
      } else {
        lv_label_set_text(s_batIcon, icon);
      }
      lv_obj_set_style_text_color(s_batIcon, col, 0);
      // Pourcentage
      char pbuf[8];
      snprintf(pbuf, sizeof(pbuf), "%d%%", pct < 0 ? 0 : pct);
      lv_label_set_text(s_batPctLabel, pbuf);
      lv_obj_set_style_text_color(s_batPctLabel, col, 0);
    }
  }
}

} // namespace

static void statsInit() { s_screen = nullptr; }

static void statsUpdate(uint32_t dtMs, Arduino_GFX* gfx) {
  // BehaviorEngine gere decay + behaviors + bouche
  BehaviorEngine::update(dtMs);
  // FaceEngine gere blink, look, expression → render avec viewport clip
  FaceEngine::update(dtMs);
  // Batterie (relecture toutes les 30s)
  GotchiBattery::update(dtMs);

  if (s_screen) {
    updateWidgets(BehaviorEngine::getStats());
  }
}

static void statsOnEnter(Arduino_GFX* gfx) {
  // Activer le viewport clip + scale pour un gotchi plus petit au centre
  FaceRenderer::setViewport(VP_X, VP_Y, VP_W, VP_H);
  FaceRenderer::setScale(FACE_SCALE);

  if (!s_screen) createUI();
  for (int i = 0; i < 4; i++) s_prevPcts[i] = -1;
  updateWidgets(BehaviorEngine::getStats());
  lv_screen_load(s_screen);
  lv_obj_invalidate(s_screen);
}

static void statsOnExit(Arduino_GFX* gfx) {
  // Restaurer le flush plein ecran + taille normale pour la face view
  FaceRenderer::clearViewport();
  FaceRenderer::resetScale();
  if (gfx) gfx->fillScreen(0x0000);
}

const View VIEW_STATS = {
  "stats", statsInit, statsUpdate, statsOnEnter, statsOnExit, true
};
