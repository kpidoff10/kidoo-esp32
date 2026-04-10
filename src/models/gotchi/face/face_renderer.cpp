/**
 * Face Renderer — Double framebuffer logique avec diff pixel-par-pixel.
 * On dessine tout (yeux + bouche) dans un buffer, on compare avec le précédent,
 * on envoie seulement les pixels qui changent. Zéro flash, zéro conflit.
 *
 * Le framebuffer ne couvre pas tout l'écran (466x466 = 434KB trop gros).
 * On utilise une zone réduite qui contient le visage : 280x200 (~112KB).
 * Deux buffers en PSRAM : current et next.
 */
#include "face_renderer.h"
#include "../config/config.h"
#include "../config/gotchi_theme.h"
#include "behavior/behavior_objects.h"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <Arduino_GFX_Library.h>
#include <esp_heap_caps.h>

extern Arduino_GFX* getGotchiGfx();

#include "behavior/behavior_engine.h"

namespace {

constexpr int16_t SCR_CX = GOTCHI_LCD_WIDTH / 2;   // 233
constexpr int16_t SCR_CY = GOTCHI_LCD_HEIGHT / 2;   // 233
constexpr int16_t EYE_GAP = 190;
constexpr int16_t LEFT_EYE_CX  = SCR_CX - EYE_GAP / 2;
constexpr int16_t RIGHT_EYE_CX = SCR_CX + EYE_GAP / 2;
constexpr int16_t EYE_CY = SCR_CY;
constexpr int16_t LOOK_RANGE_X = 40;
constexpr int16_t LOOK_RANGE_Y = 25;
constexpr int16_t MOUTH_CX = SCR_CX;
constexpr int16_t MOUTH_CY = SCR_CY + 85;

// Zone du framebuffer (couvre yeux + bouche + mouvement regard)
// Yeux : 138-328 en X, 178-288 en Y + look ±40/±25 + radius ~25
// Bouche : ~193-273 en X, 305-330 en Y
constexpr int16_t FB_X = 20;
constexpr int16_t FB_Y = 130;
constexpr int16_t FB_W = 426;   // 20 à 446 — quasi tout l'écran en largeur
constexpr int16_t FB_H = 270;   // 130 à 400 — yeux + bouche + goutte

constexpr uint16_t COL_BG = 0x0000;
// Couleurs dynamiques via GotchiTheme
#define COL_EYE    (GotchiTheme::getColors().eye)
#define COL_INNER  (GotchiTheme::getColors().inner)
#define COL_TONGUE (GotchiTheme::getColors().tongue)

Arduino_GFX* s_gfx = nullptr;
uint16_t* s_fbCurrent = nullptr;  // Frame affichée actuellement
uint16_t* s_fbNext = nullptr;     // Frame en cours de construction

// Viewport (clip du flush)
bool s_useViewport = false;
int16_t s_vpX = 0, s_vpY = 0, s_vpW = FB_W, s_vpH = FB_H;
uint16_t* s_vpBuf = nullptr;
size_t s_vpBufSize = 0;

// Scale
float s_scale = 1.0f;

EyeConfig scaleEye(const EyeConfig& e, float s) {
  return {
    (int16_t)(e.height * s), (int16_t)(e.width * s),
    (int16_t)(e.offsetX * s), (int16_t)(e.offsetY * s),
    (int16_t)(e.radiusTop * s), (int16_t)(e.radiusBottom * s),
    e.slopeTop, e.slopeBottom,
  };
}

// ============================================
// Accès pixel dans le framebuffer
// ============================================
inline void fbSetPixel(uint16_t* fb, int16_t x, int16_t y, uint16_t color) {
  if (x >= 0 && x < FB_W && y >= 0 && y < FB_H)
    fb[y * FB_W + x] = color;
}

inline void fbFillRect(uint16_t* fb, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  for (int16_t j = y; j < y + h; j++)
    for (int16_t i = x; i < x + w; i++)
      fbSetPixel(fb, i, j, color);
}

inline void fbHLine(uint16_t* fb, int16_t x, int16_t y, int16_t w, uint16_t color) {
  for (int16_t i = x; i < x + w; i++)
    fbSetPixel(fb, i, y, color);
}

inline void fbFillTriangle(uint16_t* fb, int16_t x0, int16_t y0,
                            int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
  // Scanline triangle fill (simple)
  int16_t minY = std::min({y0, y1, y2});
  int16_t maxY = std::max({y0, y1, y2});
  for (int16_t y = minY; y <= maxY; y++) {
    int16_t minX = FB_W, maxX = -1;
    // Intersect each edge with scanline y
    auto edge = [&](int16_t ax, int16_t ay, int16_t bx, int16_t by) {
      if ((ay <= y && by > y) || (by <= y && ay > y)) {
        int16_t ix = ax + (int32_t)(y - ay) * (bx - ax) / (by - ay);
        if (ix < minX) minX = ix;
        if (ix > maxX) maxX = ix;
      }
    };
    edge(x0,y0,x1,y1); edge(x1,y1,x2,y2); edge(x2,y2,x0,y0);
    if (minX <= maxX) fbHLine(fb, minX, y, maxX - minX + 1, color);
  }
}

// Simple fillRoundRect dans le framebuffer (scanline)
void fbFillRoundRect(uint16_t* fb, int16_t cx, int16_t cy, int16_t w, int16_t h, int16_t r, uint16_t color) {
  int16_t rx = cx - w/2, ry2 = cy - h/2;
  if (r > h/2) r = h/2;
  if (r > w/2) r = w/2;
  if (r < 1) r = 1;
  for (int16_t j = 0; j < h; j++) {
    int16_t indent = 0;
    if (j < r) {
      float dy = r - j - 0.5f;
      indent = r - (int16_t)sqrtf((float)(r*r) - dy*dy);
    } else if (j >= h - r) {
      float dy = j - (h - r) + 0.5f;
      indent = r - (int16_t)sqrtf((float)(r*r) - dy*dy);
    }
    fbHLine(fb, rx + indent, ry2 + j, w - 2*indent, color);
  }
}

// ============================================
// Port de esp32-eyes FillEllipseCorner dans le framebuffer
// ============================================
void fbCorner(uint16_t* fb, bool isTop, bool isLeft, int16_t cx, int16_t cy,
              int32_t rx, int32_t ry, uint16_t color) {
  if (rx < 2 || ry < 2) return;
  int32_t rx2=rx*rx, ry2=ry*ry, fx2=4*rx2, fy2=4*ry2;
  int32_t x, y, s;
  // Premier octant
  for (x=0,y=ry,s=2*ry2+rx2*(1-2*ry); ry2*x<=rx2*y; x++) {
    int16_t py = isTop ? cy-y : cy+y-1;
    if (isLeft) fbHLine(fb, cx-x, py, x+1, color);
    else        fbHLine(fb, cx, py, x+1, color);
    if (s>=0){s+=fx2*(1-y);y--;}
    s+=ry2*(4*x+6);
  }
  // Second octant — bottom utilise cy+y (pas cy+y-1) pour le BL, selon l'original
  for (x=rx,y=0,s=2*rx2+ry2*(1-2*rx); rx2*y<=ry2*x; y++) {
    int16_t py;
    if (isTop) py = cy - y;
    else if (isLeft) py = cy + y;      // BL : cy + y (original)
    else py = cy + y - 1;              // BR : cy + y - 1 (original)
    if (isLeft) fbHLine(fb, cx-x, py, x+1, color);
    else        fbHLine(fb, cx, py, x+1, color);
    if (s>=0){s+=fy2*(1-x);x--;}
    s+=rx2*(4*y+6);
  }
}

// ============================================
// Dessine une forme (oeil ou bouche) dans le framebuffer
// Coordonnées relatives au framebuffer (pas à l'écran)
// ============================================
void fbDrawShape(uint16_t* fb, int16_t centerX, int16_t centerY, const EyeConfig& cfg, uint16_t color) {
  int16_t W = cfg.width > 0 ? cfg.width : 1;
  int16_t H = cfg.height > 0 ? cfg.height : 1;
  int16_t rT = cfg.radiusTop, rB = cfg.radiusBottom;
  int16_t cX = centerX + cfg.offsetX;
  int16_t cY = centerY + cfg.offsetY;

  // Pour les petites formes, utiliser un simple fillRoundRect
  // L'algo esp32-eyes avec rectangles+triangles+coins crée des artefacts quand H est petit
  if (H <= 30) {
    int16_t r = std::min({rT, rB, (int16_t)(H/2), (int16_t)(W/2)});
    if (r < 1) r = 1;
    // fillRoundRect simple dans le framebuffer
    int16_t rx = cX - W/2, ry2 = cY - H/2;
    for (int16_t j = 0; j < H; j++) {
      // Calculer l'indentation pour les coins arrondis
      int16_t indent = 0;
      if (j < r) {
        float dy = r - j - 0.5f;
        indent = r - (int16_t)sqrtf(r*r - dy*dy);
      } else if (j >= H - r) {
        float dy = j - (H - r) + 0.5f;
        indent = r - (int16_t)sqrtf(r*r - dy*dy);
      }
      fbHLine(fb, rx + indent, ry2 + j, W - 2*indent, color);
    }
    return;
  }

  int32_t dyT = (int32_t)(cfg.slopeTop * H / 2.0f);
  int32_t dyB = (int32_t)(cfg.slopeBottom * H / 2.0f);
  int32_t totalH = H + dyT - dyB;
  if (totalH < 2) totalH = 2;

  // Clamp radius à la moitié de la dimension la plus petite
  int16_t maxR = std::min(W, (int16_t)totalH) / 2;
  if (rT > maxR) rT = maxR;
  if (rB > maxR) rB = maxR;
  if (rT > 0 && rB > 0 && totalH - 1 < rT + rB) {
    int16_t sum = rT + rB;
    rT = (int16_t)((float)rT * (totalH-1) / sum);
    rB = (int16_t)((float)rB * (totalH-1) / sum);
  }

  int32_t TLx=cX-W/2+rT, TLy=cY-H/2+rT-dyT;
  int32_t TRx=cX+W/2-rT, TRy=cY-H/2+rT+dyT;
  int32_t BLx=cX-W/2+rB, BLy=cY+H/2-rB-dyB;
  int32_t BRx=cX+W/2-rB, BRy=cY+H/2-rB+dyB;

  int32_t minCx=std::min(TLx,BLx), maxCx=std::max(TRx,BRx);
  int32_t minCy=std::min(TLy,TRy), maxCy=std::max(BLy,BRy);

  fbFillRect(fb, minCx, minCy, maxCx-minCx+1, maxCy-minCy+1, color);
  fbFillRect(fb, TRx, TRy, rB+(BRx-TRx)+1, BRy-TRy+1, color);
  fbFillRect(fb, TLx-rT, TLy, rT+(BLx-TLx)+1, BLy-TLy+1, color);
  fbFillRect(fb, TLx, TLy-rT, TRx-TLx+1, rT+(TRy-TLy)+1, color);
  fbFillRect(fb, BLx, BLy, BRx-BLx+1, rB+(BRy-BLy)+1, color);

  if (dyT > 0) {
    fbFillTriangle(fb, TLx,TLy-rT, TRx,TRy-rT, TRx,TLy-rT, COL_BG);
    fbFillTriangle(fb, TRx,TRy-rT, TLx,TLy-rT, TLx,TRy-rT, color);
  } else if (dyT < 0) {
    fbFillTriangle(fb, TRx,TRy-rT, TLx,TLy-rT, TLx,TRy-rT, COL_BG);
    fbFillTriangle(fb, TLx,TLy-rT, TRx,TRy-rT, TRx,TLy-rT, color);
  }
  if (dyB > 0) {
    fbFillTriangle(fb, BRx+rB,BRy+rB, BLx-rB,BLy+rB, BLx-rB,BRy+rB, COL_BG);
    fbFillTriangle(fb, BLx-rB,BLy+rB, BRx+rB,BRy+rB, BRx+rB,BLy+rB, color);
  } else if (dyB < 0) {
    fbFillTriangle(fb, BLx-rB,BLy+rB, BRx+rB,BRy+rB, BRx+rB,BLy+rB, COL_BG);
    fbFillTriangle(fb, BRx+rB,BRy+rB, BLx-rB,BLy+rB, BLx-rB,BRy+rB, color);
  }

  if (rT > 0) {
    fbCorner(fb, true, true, TLx, TLy, rT, rT, color);
    fbCorner(fb, true, false, TRx, TRy, rT, rT, color);
  }
  if (rB > 0) {
    fbCorner(fb, false, true, BLx, BLy, rB, rB, color);
    fbCorner(fb, false, false, BRx, BRy, rB, rB, color);
  }
}

// ============================================
// Envoie les pixels qui ont changé entre current et next
// Envoie par lignes continues (runs) pour minimiser les appels SPI
// ============================================
void flushBuffer() {
  if (s_useViewport && s_vpW > 0 && s_vpH > 0) {
    // Flush seulement la sous-region viewport
    size_t needed = s_vpW * s_vpH * sizeof(uint16_t);
    if (needed > s_vpBufSize) {
      if (s_vpBuf) heap_caps_free(s_vpBuf);
      s_vpBuf = (uint16_t*)heap_caps_malloc(needed, MALLOC_CAP_SPIRAM);
      s_vpBufSize = s_vpBuf ? needed : 0;
    }
    if (s_vpBuf) {
      for (int16_t y = 0; y < s_vpH; y++) {
        memcpy(s_vpBuf + y * s_vpW,
               s_fbNext + (s_vpY + y) * FB_W + s_vpX,
               s_vpW * sizeof(uint16_t));
      }
      s_gfx->draw16bitRGBBitmap(FB_X + s_vpX, FB_Y + s_vpY, s_vpBuf, s_vpW, s_vpH);
    }
  } else {
    s_gfx->draw16bitRGBBitmap(FB_X, FB_Y, s_fbNext, FB_W, FB_H);
  }

  // Swap
  uint16_t* tmp = s_fbCurrent;
  s_fbCurrent = s_fbNext;
  s_fbNext = tmp;
}

} // namespace

namespace FaceRenderer {

void init() {
  s_gfx = getGotchiGfx();
  if (!s_gfx) return;

  // Allouer 2 framebuffers en PSRAM
  size_t fbSize = FB_W * FB_H * sizeof(uint16_t);
  s_fbCurrent = (uint16_t*)heap_caps_malloc(fbSize, MALLOC_CAP_SPIRAM);
  s_fbNext    = (uint16_t*)heap_caps_malloc(fbSize, MALLOC_CAP_SPIRAM);
  if (!s_fbCurrent || !s_fbNext) {
    Serial.println("[FACE] ERREUR: allocation framebuffer PSRAM échouée");
    return;
  }
  Serial.printf("[FACE] Framebuffers alloués: 2x %dx%d = %u KB\n", FB_W, FB_H, (unsigned)(fbSize*2/1024));

  memset(s_fbCurrent, 0, fbSize);
  memset(s_fbNext, 0, fbSize);

  s_gfx->fillScreen(COL_BG);

  // Dessiner la frame initiale
  FacePreset n = FacePresets::getPreset(FaceExpression::Normal);
  int16_t lcx = LEFT_EYE_CX - FB_X;
  int16_t rcx = RIGHT_EYE_CX - FB_X;
  int16_t ecy = EYE_CY - FB_Y;

  fbDrawShape(s_fbCurrent, lcx, ecy, n.left, COL_EYE);
  fbDrawShape(s_fbCurrent, rcx, ecy, n.right, COL_EYE);
  // Pas de bouche par défaut (affichée seulement si mouthState sort de la zone neutre)

  // Envoyer toute la frame initiale d'un coup
  s_gfx->draw16bitRGBBitmap(FB_X, FB_Y, s_fbCurrent, FB_W, FB_H);
  memcpy(s_fbNext, s_fbCurrent, fbSize);
}

void render(const EyeConfig& left, const EyeConfig& right, float lookX, float lookY, float mouthState) {
  if (!s_gfx || !s_fbCurrent || !s_fbNext) return;

  float sc = s_scale;
  // Centre du FB (invariant au scale)
  int16_t fbCX = SCR_CX - FB_X;  // 213
  int16_t fbCY = EYE_CY - FB_Y;  // 103

  // Positions des yeux (scalees par rapport au centre)
  int16_t halfGap = (int16_t)(EYE_GAP / 2 * sc);
  int16_t lrx = (int16_t)(lookX * LOOK_RANGE_X * sc);
  int16_t lry = (int16_t)(lookY * LOOK_RANGE_Y * sc);
  int16_t lcx = fbCX - halfGap + lrx;
  int16_t rcx = fbCX + halfGap + lrx;
  int16_t ecy = fbCY + lry;

  // Bouche (scalee par rapport aux yeux)
  int16_t mouthOff = (int16_t)((MOUTH_CY - EYE_CY) * sc);
  int16_t mcx = fbCX;
  int16_t mcy = fbCY + mouthOff + lry;

  // Effacer le next buffer
  memset(s_fbNext, 0, FB_W * FB_H * sizeof(uint16_t));

  // Dessiner les 2 yeux (scaled)
  EyeConfig sl = sc < 1.0f ? scaleEye(left, sc) : left;
  EyeConfig sr = sc < 1.0f ? scaleEye(right, sc) : right;
  fbDrawShape(s_fbNext, lcx, ecy, sl, COL_EYE);
  fbDrawShape(s_fbNext, rcx, ecy, sr, COL_EYE);

  // Bouche (scaled)
  if (mouthState < -0.15f) {
    float o = -mouthState;
    int16_t outerH = (int16_t)((16 + o * 22) * sc);
    int16_t outerW = (int16_t)((36 + o * 36) * sc);
    int16_t outerR = outerH / 2;
    EyeConfig outerCfg = { outerH, outerW, 0, 0, outerR, outerR, 0, 0 };
    fbDrawShape(s_fbNext, mcx, mcy, outerCfg, COL_EYE);

    int16_t innerW = outerW - (int16_t)(12 * sc);
    int16_t innerH = outerH - (int16_t)(10 * sc);
    if (innerW < 4) innerW = 4;
    if (innerH < 3) innerH = 3;
    fbFillRoundRect(s_fbNext, mcx, mcy + (int16_t)(2*sc), innerW, innerH, innerH/2, COL_INNER);

    int16_t tongueW = innerW * 2 / 3;
    int16_t tongueH = innerH / 2;
    if (tongueH > 2) {
      fbFillRoundRect(s_fbNext, mcx, mcy + (int16_t)(2*sc) + innerH/2 - tongueH/2,
                      tongueW, tongueH, tongueH/2, COL_TONGUE);
    }
  } else if (mouthState > 0.1f) {
    int16_t w = (int16_t)((30 + mouthState * 20) * sc);
    int16_t h = (int16_t)((6 + mouthState * 4) * sc);
    if (h < 2) h = 2;
    EyeConfig mCfg = { h, w, 0, 0, 3, 3, 0, 0 };
    fbDrawShape(s_fbNext, mcx, mcy, mCfg, COL_EYE);
  }

  // Goutte de bave (seulement en mode normal, pas en viewport)
  if (!s_useViewport) {
    float drool = BehaviorEngine::getStats().droolLength;
    float retract = BehaviorEngine::getStats().droolRetract;
    if (drool > 1.0f) {
      int16_t droolX = mcx + 6;
      int16_t droolTopY = mcy + 12;
      int16_t droolLen = (int16_t)drool;
      int16_t retractPx = (int16_t)retract;
      int16_t filStart = droolTopY + retractPx;
      int16_t filEnd = droolTopY + droolLen;
      if (filEnd > filStart) {
        int16_t filW = 3 - retractPx / 15;
        if (filW < 1) filW = 1;
        fbFillRect(s_fbNext, droolX - filW/2, filStart, filW, filEnd - filStart, COL_EYE);
      }
      int16_t dropR = 3 + (droolLen - retractPx) / 10;
      if (dropR > 8) dropR = 8;
      if (dropR < 2) dropR = 2;
      int16_t dropY = filEnd;
      if (dropY + dropR < FB_H) {
        for (int16_t dy = -dropR; dy <= dropR; dy++) {
          int16_t dx = (int16_t)sqrtf((float)(dropR*dropR - dy*dy));
          fbHLine(s_fbNext, droolX - dx, dropY + dy, 2*dx + 1, COL_EYE);
        }
      }
    }
  }

  // Objets visuels (balle, coeurs) seulement en mode normal
  if (!s_useViewport) {
    BehaviorObjects::draw();
  }

  flushBuffer();
}

uint16_t* getNextBuffer() { return s_fbNext; }
int16_t getFbX() { return FB_X; }
int16_t getFbY() { return FB_Y; }
int16_t getFbW() { return FB_W; }
int16_t getFbH() { return FB_H; }

void setViewport(int16_t x, int16_t y, int16_t w, int16_t h) {
  s_vpX = x; s_vpY = y; s_vpW = w; s_vpH = h;
  s_useViewport = true;
}

void clearViewport() {
  s_useViewport = false;
  s_vpX = 0; s_vpY = 0; s_vpW = FB_W; s_vpH = FB_H;
}

void setScale(float scale) { s_scale = scale; }
void resetScale() { s_scale = 1.0f; }

} // namespace FaceRenderer
