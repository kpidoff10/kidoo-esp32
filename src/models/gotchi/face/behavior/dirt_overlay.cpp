#include "dirt_overlay.h"
#include "behavior_engine.h"
#include "../face_engine.h"
#include "sprites/sprite_dirt_big_0.h"
#include "sprites/sprite_dirt_big_1.h"
#include "sprites/sprite_dirt_big_2.h"
#include "sprites/sprite_dirt_big_3.h"
#include "sprites/sprite_sponge_emoji_64.h"
#include "sprites/sprite_toothbrush_emoji_88.h"
#include "sprites/sprite_sparkle_26.h"
#include "sprites/sprite_bubbles_emoji_22.h"
#include "behavior_objects.h"
#include "sprites/sprite_asset.h"
#include <pgmspace.h>
#include <cstring>
#include <cstdlib>
#include <cmath>

namespace {

constexpr int MAX_SPOTS = 15;
constexpr int HIT_RADIUS = 45;       // Rayon de détection frottement (px)
constexpr float CLEAN_THRESHOLD = 0.70f;
constexpr uint8_t MAX_OPACITY = 3;   // Niveaux d'opacité : 3=plein, 2=atténué, 1=faible, 0=disparu

struct DirtSpot {
  int16_t x, y;
  uint8_t tileIdx;
  uint8_t opacity;    // 0=propre, 1-3=sale (3=le plus sale)
};

DirtSpot s_spots[MAX_SPOTS];
int s_totalDirty = 0;
int s_currentDirty = 0;
bool s_washMode = false;
bool s_hasDirt = false;
uint32_t s_washCooldown = 0;  // Cooldown post-nettoyage (ms)

// Position du doigt pour l'éponge (cible + position lissée)
float s_fingerX = -100;      // Position lissée (affichage)
float s_fingerY = -100;
float s_targetX = -100;      // Position cible (touch brut)
float s_targetY = -100;
bool s_fingerActive = false;
constexpr int16_t SPONGE_SIZE = 64;

// --- Brush mode (brossage de dents) ---
bool s_brushMode = false;
float s_brushX = -100, s_brushY = -100;
float s_brushTargetX = -100, s_brushTargetY = -100;
bool s_brushActive = false;
float s_brushDistance = 0;              // Distance cumulative de frottement
constexpr float BRUSH_DONE_DIST = 3000.0f;  // Distance pour terminer le brossage
constexpr float BRUSH_MOUTH_CX = 233.0f;
constexpr float BRUSH_MOUTH_CY = 318.0f;
constexpr float BRUSH_ZONE_RADIUS = 80.0f;  // Zone efficace autour de la bouche
uint32_t s_brushBubbleTimer = 0;
uint32_t s_brushCooldown = 0;

const SpriteAsset* DIRT_TILES[] = {
  &SPRITE_DIRT_BIG_0_ASSET,
  &SPRITE_DIRT_BIG_1_ASSET,
  &SPRITE_DIRT_BIG_2_ASSET,
  &SPRITE_DIRT_BIG_3_ASSET,
};
constexpr int NUM_TILES = 4;

constexpr int16_t SCR_W = 466;
constexpr int16_t SCR_H = 466;
constexpr int16_t TOP_BUF_X = 20;
constexpr int16_t TOP_BUF_Y = 30;
constexpr int16_t TOP_BUF_W = 426;
constexpr int16_t TOP_BUF_H = 100;

// Alpha blend avec facteur d'opacité global
inline uint16_t blend565(uint16_t bg, uint16_t fg, uint8_t a) {
  if (a == 0) return bg;
  if (a >= 250) return fg;
  uint8_t inv = 255 - a;
  uint16_t r = (((fg >> 11) & 0x1F) * a + ((bg >> 11) & 0x1F) * inv) / 255;
  uint16_t g = (((fg >> 5) & 0x3F) * a + ((bg >> 5) & 0x3F) * inv) / 255;
  uint16_t b = ((fg & 0x1F) * a + (bg & 0x1F) * inv) / 255;
  return (r << 11) | (g << 5) | b;
}

// Blit un sprite avec un facteur d'opacité (1-3 → 33%-100%)
void blitTile(uint16_t* buf, int16_t bufW, int16_t bufH,
              int16_t bufOffX, int16_t bufOffY,
              int16_t spotX, int16_t spotY,
              const SpriteAsset& tile, uint8_t opacity) {
  const int16_t tw = tile.width;
  const int16_t th = tile.height;
  const int16_t x0 = spotX - tw / 2;
  const int16_t y0 = spotY - th / 2;
  const bool hasRgb = (tile.rgb565 != nullptr);
  // Facteur d'opacité : 1=50%, 2=75%, 3=100%
  static const uint8_t opacityLevels[] = {0, 128, 192, 255};
  uint16_t opacityScale = opacityLevels[opacity > 3 ? 3 : opacity];

  for (int16_t dy = 0; dy < th; dy++) {
    int16_t sy = y0 + dy;
    int16_t py = sy - bufOffY;
    if (py < 0 || py >= bufH) continue;
    const int rowOff = dy * tw;
    for (int16_t dx = 0; dx < tw; dx++) {
      int16_t sx = x0 + dx;
      int16_t px = sx - bufOffX;
      if (px < 0 || px >= bufW) continue;
      uint8_t a = pgm_read_byte(&tile.alpha[rowOff + dx]);
      if (a == 0) continue;
      // Appliquer le facteur d'opacité
      a = (uint8_t)((uint16_t)a * opacityScale / 255);
      if (a == 0) continue;
      uint16_t src = hasRgb ? pgm_read_word(&tile.rgb565[rowOff + dx]) : 0x4A28;
      uint16_t* p = &buf[py * bufW + px];
      *p = blend565(*p, src, a);
    }
  }
}

// Position aléatoire dans l'écran rond
void randomSpotPosition(int16_t& x, int16_t& y) {
  const int16_t cx = SCR_W / 2;
  const int16_t cy = SCR_H / 2;
  const int16_t maxR = 190;

  for (int attempt = 0; attempt < 10; attempt++) {
    x = 60 + rand() % (SCR_W - 120);
    y = 60 + rand() % (SCR_H - 120);
    int32_t dx = x - cx;
    int32_t dy = y - cy;
    if (dx * dx + dy * dy < (int32_t)maxR * maxR) return;
  }
  x = cx + (rand() % 100) - 50;
  y = cy + (rand() % 100) - 50;
}

} // namespace

namespace DirtOverlay {

void setDirty(float hygiene) {
  float ratio = (40.0f - hygiene) / 40.0f;
  if (ratio <= 0) { clear(); return; }
  if (ratio > 1.0f) ratio = 1.0f;

  int numSpots = (int)(ratio * MAX_SPOTS);
  if (numSpots < 3) numSpots = 3;

  memset(s_spots, 0, sizeof(s_spots));
  s_totalDirty = 0;
  s_currentDirty = 0;

  for (int i = 0; i < numSpots && i < MAX_SPOTS; i++) {
    int16_t x, y;
    randomSpotPosition(x, y);
    s_spots[i].x = x;
    s_spots[i].y = y;
    s_spots[i].tileIdx = rand() % NUM_TILES;
    s_spots[i].opacity = MAX_OPACITY;
    s_totalDirty++;
    s_currentDirty++;
  }

  s_hasDirt = (s_totalDirty > 0);
}

void clear() {
  for (int i = 0; i < MAX_SPOTS; i++) s_spots[i].opacity = 0;
  s_totalDirty = 0;
  s_currentDirty = 0;
  s_hasDirt = false;
  s_washMode = false;
  s_fingerActive = false;
}

void setWashMode(bool enabled) {
  s_washMode = enabled;
  if (enabled) {
    // Afficher l'éponge au centre dès l'activation
    s_fingerX = 233.0f;
    s_fingerY = 300.0f;
    s_targetX = 233.0f;
    s_targetY = 300.0f;
    s_fingerActive = true;
  } else {
    s_fingerActive = false;
  }
}

bool isWashMode() {
  return s_washMode || s_washCooldown > 0;
}

void update(uint32_t dtMs) {
  // Cooldowns
  if (s_washCooldown > 0) {
    if (dtMs >= s_washCooldown) s_washCooldown = 0;
    else s_washCooldown -= dtMs;
  }
  if (s_brushCooldown > 0) {
    if (dtMs >= s_brushCooldown) s_brushCooldown = 0;
    else s_brushCooldown -= dtMs;
  }

  // Interpolation éponge
  if (s_fingerActive && s_washMode) {
    constexpr float LERP = 0.4f;
    s_fingerX += (s_targetX - s_fingerX) * LERP;
    s_fingerY += (s_targetY - s_fingerY) * LERP;
  }

  // Interpolation brosse à dents
  if (s_brushActive && s_brushMode) {
    constexpr float LERP = 0.4f;
    s_brushX += (s_brushTargetX - s_brushX) * LERP;
    s_brushY += (s_brushTargetY - s_brushY) * LERP;
  }

  // Auto-génération des taches quand l'hygiène passe sous 40
  float hygiene = BehaviorEngine::getStats().hygiene;
  if (!s_hasDirt && !s_washMode && s_washCooldown == 0 && hygiene < 40.0f) {
    setDirty(hygiene);
  }
}

bool onFingerMove(float screenX, float screenY) {
  if (!s_washMode || !s_hasDirt || s_washCooldown > 0) return false;

  s_targetX = screenX;
  s_targetY = screenY;
  if (!s_fingerActive) {
    // Premier touch : snap direct, pas d'interpolation
    s_fingerX = screenX;
    s_fingerY = screenY;
  }
  s_fingerActive = true;

  // Le gotchi regarde où on frotte (coordonnées normalisées -1 à +1)
  float lookX = (screenX - 233.0f) / 233.0f;
  float lookY = (screenY - 233.0f) / 233.0f;
  if (lookX > 1.0f) lookX = 1.0f;
  if (lookX < -1.0f) lookX = -1.0f;
  if (lookY > 1.0f) lookY = 1.0f;
  if (lookY < -1.0f) lookY = -1.0f;
  FaceEngine::lookAtForced(lookX, lookY, 100);

  bool changed = false;
  for (int i = 0; i < MAX_SPOTS; i++) {
    if (s_spots[i].opacity == 0) continue;
    float dx = screenX - s_spots[i].x;
    float dy = screenY - s_spots[i].y;
    if (dx * dx + dy * dy < HIT_RADIUS * HIT_RADIUS) {
      // Atténuer la tache d'un niveau
      s_spots[i].opacity--;
      if (s_spots[i].opacity == 0) {
        s_currentDirty--;
      }
      changed = true;
    }
  }

  if (s_totalDirty > 0 && changed) {
    float cleanedRatio = 1.0f - (float)s_currentDirty / (float)s_totalDirty;
    if (cleanedRatio >= CLEAN_THRESHOLD) {
      clear();
      s_washCooldown = 2000;  // Bloquer les swipes encore 2s

      BehaviorEngine::clean();

      // Hygiène à 100% (après clean qui met +40)
      BehaviorEngine::getStats().hygiene = 100.0f;

      // Sparkles de propreté ✨ (après clean pour ne pas être détruites)
      for (int i = 0; i < 5; i++) {
        float sx = 100.0f + i * 80.0f + (rand() % 20);
        float sy = 180.0f + (rand() % 60);
        BehaviorObjects::spawnSprite(SPRITE_SPARKLE_26_ASSET, 0,
          sx, sy, ((rand() % 20) - 10) / 1000.0f, -0.05f,
          0, 0, false, 2500);
      }
      return true;
    }
  }

  return false;
}

void drawIntoFB(uint16_t* fb, int16_t fbW, int16_t fbH, int16_t fbX, int16_t fbY) {
  if (!s_hasDirt || !fb) return;

  // Taches de saleté avec opacité progressive
  for (int i = 0; i < MAX_SPOTS; i++) {
    if (s_spots[i].opacity == 0) continue;
    const SpriteAsset& tile = *DIRT_TILES[s_spots[i].tileIdx];
    blitTile(fb, fbW, fbH, fbX, fbY,
             s_spots[i].x, s_spots[i].y, tile, s_spots[i].opacity);
  }

  // Éponge dessinée séparément via drawSponge()
}

void drawIntoTopBuf(uint16_t* buf, int16_t bufW, int16_t bufH) {
  if (!s_hasDirt || !buf) return;

  for (int i = 0; i < MAX_SPOTS; i++) {
    if (s_spots[i].opacity == 0) continue;
    int16_t spotTop = s_spots[i].y - 32;
    if (spotTop >= TOP_BUF_Y + TOP_BUF_H) continue;
    const SpriteAsset& tile = *DIRT_TILES[s_spots[i].tileIdx];
    blitTile(buf, bufW, bufH, TOP_BUF_X, TOP_BUF_Y,
             s_spots[i].x, s_spots[i].y, tile, s_spots[i].opacity);
  }

  // Éponge dessinée séparément via drawSponge()
}

void drawSpongeIntoFB(uint16_t* fb, int16_t fbW, int16_t fbH, int16_t fbX, int16_t fbY) {
  if (!s_washMode || !s_fingerActive || !fb) return;
  blitTile(fb, fbW, fbH, fbX, fbY,
           (int16_t)s_fingerX, (int16_t)s_fingerY,
           SPRITE_SPONGE_EMOJI_64_ASSET, MAX_OPACITY);
}

void drawSpongeIntoTopBuf(uint16_t* buf, int16_t bufW, int16_t bufH) {
  if (!s_washMode || !s_fingerActive || !buf) return;
  // Seulement si l'éponge touche la zone top (y < 130)
  if ((int16_t)s_fingerY - SPONGE_SIZE / 2 >= TOP_BUF_Y + TOP_BUF_H) return;
  blitTile(buf, bufW, bufH, TOP_BUF_X, TOP_BUF_Y,
           (int16_t)s_fingerX, (int16_t)s_fingerY,
           SPRITE_SPONGE_EMOJI_64_ASSET, MAX_OPACITY);
}

// --- Mode brossage de dents ---

void setBrushMode(bool enabled) {
  s_brushMode = enabled;
  if (enabled) {
    s_brushX = 233.0f;
    s_brushY = 300.0f;
    s_brushTargetX = 233.0f;
    s_brushTargetY = 300.0f;
    s_brushActive = true;
    s_brushDistance = 0;
    s_brushBubbleTimer = 0;
    s_brushCooldown = 0;
    // Ouvrir la bouche
    BehaviorEngine::getStats().mouthState = -0.3f;
  } else {
    s_brushActive = false;
    BehaviorEngine::getStats().mouthState = 0.5f;
  }
}

bool isBrushMode() {
  return s_brushMode || s_brushCooldown > 0;
}

bool onBrushFingerMove(float screenX, float screenY) {
  if (!s_brushMode || s_brushCooldown > 0) return false;

  float prevTargetX = s_brushTargetX;
  float prevTargetY = s_brushTargetY;
  s_brushTargetX = screenX;
  s_brushTargetY = screenY;
  if (!s_brushActive) {
    s_brushX = screenX;
    s_brushY = screenY;
  }
  s_brushActive = true;

  // Le gotchi regarde la brosse
  float lookX = (screenX - 233.0f) / 233.0f;
  float lookY = (screenY - 233.0f) / 233.0f;
  if (lookX > 1.0f) lookX = 1.0f;
  if (lookX < -1.0f) lookX = -1.0f;
  if (lookY > 1.0f) lookY = 1.0f;
  if (lookY < -1.0f) lookY = -1.0f;
  FaceEngine::lookAtForced(lookX, lookY, 100);

  // Garder la bouche bien ouverte pendant le brossage
  BehaviorEngine::getStats().mouthState = -0.7f;

  // Compter la distance seulement si dans la zone de la bouche
  float dxMouth = screenX - BRUSH_MOUTH_CX;
  float dyMouth = screenY - BRUSH_MOUTH_CY;
  if (dxMouth * dxMouth + dyMouth * dyMouth < BRUSH_ZONE_RADIUS * BRUSH_ZONE_RADIUS) {
    float dx = screenX - prevTargetX;
    float dy = screenY - prevTargetY;
    float dist = dx * dx + dy * dy;
    if (dist > 4.0f) {  // Filtre bruit
      s_brushDistance += sqrtf(dist);

      // Spawn emoji bulles 🫧 de mousse de temps en temps
      s_brushBubbleTimer++;
      if (s_brushBubbleTimer % 3 == 0) {
        float bx = BRUSH_MOUTH_CX + (rand() % 80) - 40;
        float by = BRUSH_MOUTH_CY + (rand() % 40) - 20;
        BehaviorObjects::spawnSprite(SPRITE_BUBBLES_EMOJI_22_ASSET, 0,
          bx, by, ((rand() % 20) - 10) / 1000.0f, -0.04f,
          0, 0, false, 1800);
      }
    }

    // Brossage terminé ?
    if (s_brushDistance >= BRUSH_DONE_DIST) {
      // Sparkles ✨
      for (int i = 0; i < 5; i++) {
        float sx = 100.0f + i * 80.0f + (rand() % 20);
        float sy = 200.0f + (rand() % 60);
        BehaviorObjects::spawnSprite(SPRITE_SPARKLE_26_ASSET, 0,
          sx, sy, ((rand() % 20) - 10) / 1000.0f, -0.05f,
          0, 0, false, 2500);
      }

      // Boost hygiène
      auto& stats = BehaviorEngine::getStats();
      stats.hygiene += 20.0f;
      if (stats.hygiene > 100.0f) stats.hygiene = 100.0f;
      stats.happiness += 5.0f;
      stats.clamp();

      s_brushMode = false;
      s_brushActive = false;
      s_brushCooldown = 2000;
      stats.mouthState = 0.6f;  // Sourire
      return true;
    }
  }

  return false;
}

void drawBrushIntoFB(uint16_t* fb, int16_t fbW, int16_t fbH, int16_t fbX, int16_t fbY) {
  if (!s_brushMode || !s_brushActive || !fb) return;
  blitTile(fb, fbW, fbH, fbX, fbY,
           (int16_t)s_brushX, (int16_t)s_brushY,
           SPRITE_TOOTHBRUSH_EMOJI_88_ASSET, MAX_OPACITY);
}

void drawBrushIntoTopBuf(uint16_t* buf, int16_t bufW, int16_t bufH) {
  if (!s_brushMode || !s_brushActive || !buf) return;
  if ((int16_t)s_brushY - 32 >= TOP_BUF_Y + TOP_BUF_H) return;
  blitTile(buf, bufW, bufH, TOP_BUF_X, TOP_BUF_Y,
           (int16_t)s_brushX, (int16_t)s_brushY,
           SPRITE_TOOTHBRUSH_EMOJI_88_ASSET, MAX_OPACITY);
}

bool isDirty() {
  return s_hasDirt;
}

} // namespace DirtOverlay
