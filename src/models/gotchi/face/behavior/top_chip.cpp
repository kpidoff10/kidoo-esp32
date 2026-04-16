#include "top_chip.h"
#include <cstring>
#include <Arduino_GFX_Library.h>
#include <esp_heap_caps.h>

extern Arduino_GFX* getGotchiGfx();

namespace {

bool s_visible = false;
char s_text[32] = "";
uint16_t s_accentColor = 0xFFFF;
uint16_t s_bgColor = 0x1082;

// Mini-framebuffer pour le chip (alloué dynamiquement)
uint16_t* s_chipBuf = nullptr;
int16_t s_chipW = 0;
int16_t s_chipH = 0;
int16_t s_chipScreenX = 0;
int16_t s_chipScreenY = 0;
bool s_needsClear = false;  // Flag pour effacer la zone à la prochaine frame

// Constantes d'affichage
constexpr int SCALE = 3;
constexpr int CHAR_W = 5 * SCALE + SCALE;  // largeur char + espacement
constexpr int CHAR_H = 7 * SCALE;
constexpr int PAD_X = 14;
constexpr int PAD_Y = 10;
constexpr int RADIUS = 12;
constexpr int16_t SCREEN_CX = 233;  // Centre écran
constexpr int16_t CHIP_Y = 55;      // Position Y du chip en haut de l'écran

// ---- Font bitmap 5x7 ----
static const uint8_t FONT_5X7[][5] = {
  // 0-9
  {0x3E,0x51,0x49,0x45,0x3E}, // 0
  {0x00,0x42,0x7F,0x40,0x00}, // 1
  {0x42,0x61,0x51,0x49,0x46}, // 2
  {0x21,0x41,0x45,0x4B,0x31}, // 3
  {0x18,0x14,0x12,0x7F,0x10}, // 4
  {0x27,0x45,0x45,0x45,0x39}, // 5
  {0x3C,0x4A,0x49,0x49,0x30}, // 6
  {0x01,0x71,0x09,0x05,0x03}, // 7
  {0x36,0x49,0x49,0x49,0x36}, // 8
  {0x06,0x49,0x49,0x29,0x1E}, // 9
  // Symboles (index 10+)
  {0x00,0x60,0x60,0x00,0x00}, // 10 = '.'
  {0x06,0x09,0x09,0x06,0x00}, // 11 = '°'
  {0x00,0x00,0x00,0x00,0x00}, // 12 = ' '
  {0x00,0x14,0x14,0x00,0x00}, // 13 = ':'
  {0x08,0x08,0x08,0x08,0x08}, // 14 = '-'
  {0x23,0x13,0x08,0x64,0x62}, // 15 = '%'
  // Lettres A-Z (index 16+)
  {0x7E,0x11,0x11,0x11,0x7E}, // 16 = A
  {0x7F,0x49,0x49,0x49,0x36}, // 17 = B
  {0x3E,0x41,0x41,0x41,0x22}, // 18 = C
  {0x7F,0x41,0x41,0x22,0x1C}, // 19 = D
  {0x7F,0x49,0x49,0x49,0x41}, // 20 = E
  {0x7F,0x09,0x09,0x09,0x01}, // 21 = F
  {0x3E,0x41,0x49,0x49,0x7A}, // 22 = G
  {0x7F,0x08,0x08,0x08,0x7F}, // 23 = H
  {0x00,0x41,0x7F,0x41,0x00}, // 24 = I
  {0x20,0x40,0x41,0x3F,0x01}, // 25 = J
  {0x7F,0x08,0x14,0x22,0x41}, // 26 = K
  {0x7F,0x40,0x40,0x40,0x40}, // 27 = L
  {0x7F,0x02,0x0C,0x02,0x7F}, // 28 = M
  {0x7F,0x04,0x08,0x10,0x7F}, // 29 = N
  {0x3E,0x41,0x41,0x41,0x3E}, // 30 = O
  {0x7F,0x09,0x09,0x09,0x06}, // 31 = P
  {0x3E,0x41,0x51,0x21,0x5E}, // 32 = Q
  {0x7F,0x09,0x19,0x29,0x46}, // 33 = R
  {0x46,0x49,0x49,0x49,0x31}, // 34 = S
  {0x01,0x01,0x7F,0x01,0x01}, // 35 = T
  {0x3F,0x40,0x40,0x40,0x3F}, // 36 = U
  {0x1F,0x20,0x40,0x20,0x1F}, // 37 = V
  {0x3F,0x40,0x38,0x40,0x3F}, // 38 = W
  {0x63,0x14,0x08,0x14,0x63}, // 39 = X
  {0x07,0x08,0x70,0x08,0x07}, // 40 = Y
  {0x61,0x51,0x49,0x45,0x43}, // 41 = Z
};

int charToFontIndex(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c == '.') return 10;
  if (c == '\xB0' || c == '^') return 11;  // °
  if (c == ' ') return 12;
  if (c == ':') return 13;
  if (c == '-') return 14;
  if (c == '%') return 15;
  if (c >= 'A' && c <= 'Z') return 16 + (c - 'A');
  if (c >= 'a' && c <= 'z') return 16 + (c - 'a');
  return 12;  // espace par défaut
}

inline void bufPx(uint16_t* buf, int16_t w, int16_t h, int16_t x, int16_t y, uint16_t color) {
  if (x >= 0 && x < w && y >= 0 && y < h)
    buf[y * w + x] = color;
}

void fillRoundRect(uint16_t* buf, int16_t bufW, int16_t bufH,
                   int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
  for (int16_t row = 0; row < h; row++) {
    int16_t inset = 0;
    if (row < r) {
      // Coin haut : calculer l'arrondi
      int16_t dy = r - row;
      while (inset * inset + dy * dy > r * r) inset++;
    } else if (row >= h - r) {
      // Coin bas
      int16_t dy = row - (h - 1 - r);
      while (inset * inset + dy * dy > r * r) inset++;
    }
    for (int16_t col = inset; col < w - inset; col++) {
      bufPx(buf, bufW, bufH, x + col, y + row, color);
    }
  }
}

void drawChar(uint16_t* buf, int16_t bufW, int16_t bufH,
              int16_t x, int16_t y, int fontIdx, uint16_t color, int scale) {
  if (fontIdx < 0 || fontIdx >= (int)(sizeof(FONT_5X7) / sizeof(FONT_5X7[0]))) return;
  const uint8_t* glyph = FONT_5X7[fontIdx];
  for (int col = 0; col < 5; col++) {
    uint8_t bits = glyph[col];
    for (int row = 0; row < 7; row++) {
      if (bits & (1 << row)) {
        for (int sy = 0; sy < scale; sy++) {
          for (int sx = 0; sx < scale; sx++) {
            bufPx(buf, bufW, bufH, x + col * scale + sx, y + row * scale + sy, color);
          }
        }
      }
    }
  }
}

// Allouer/redimensionner le buffer du chip si nécessaire
bool ensureBuffer(int16_t w, int16_t h) {
  if (s_chipBuf && s_chipW == w && s_chipH == h) return true;
  if (s_chipBuf) { free(s_chipBuf); s_chipBuf = nullptr; }
  s_chipBuf = (uint16_t*)heap_caps_malloc(w * h * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
  if (!s_chipBuf) return false;
  s_chipW = w;
  s_chipH = h;
  return true;
}

// Rendre le chip dans son buffer et le flusher à l'écran
void renderAndFlush() {
  if (!s_visible || s_text[0] == '\0') return;

  int textLen = strlen(s_text);
  int textW = textLen * CHAR_W - SCALE;
  int16_t chipW = textW + PAD_X * 2;
  int16_t chipH = CHAR_H + PAD_Y * 2;

  if (!ensureBuffer(chipW, chipH)) return;

  // Centrer sur l'écran
  s_chipScreenX = SCREEN_CX - chipW / 2;
  s_chipScreenY = CHIP_Y;

  // Clear buffer
  memset(s_chipBuf, 0, chipW * chipH * sizeof(uint16_t));

  // Fond arrondi
  fillRoundRect(s_chipBuf, chipW, chipH, 0, 0, chipW, chipH, RADIUS, s_bgColor);

  // Texte
  int16_t textX = PAD_X;
  int16_t textY = PAD_Y;
  for (int i = 0; i < textLen; i++) {
    int idx = charToFontIndex(s_text[i]);
    drawChar(s_chipBuf, chipW, chipH, textX, textY, idx, s_accentColor, SCALE);
    textX += CHAR_W;
  }

  // Flush directement à l'écran
  Arduino_GFX* gfx = getGotchiGfx();
  if (gfx) {
    gfx->draw16bitRGBBitmap(s_chipScreenX, s_chipScreenY, s_chipBuf, chipW, chipH);
  }
}

// Effacer la zone du chip à l'écran (remplir de noir)
void clearFromScreen() {
  if (s_chipW <= 0 || s_chipH <= 0) return;
  Arduino_GFX* gfx = getGotchiGfx();
  if (!gfx) return;

  // Remplir la zone avec du noir
  if (s_chipBuf) {
    memset(s_chipBuf, 0, s_chipW * s_chipH * sizeof(uint16_t));
    gfx->draw16bitRGBBitmap(s_chipScreenX, s_chipScreenY, s_chipBuf, s_chipW, s_chipH);
  }
}

} // namespace

namespace TopChip {

uint16_t darken(uint16_t color) {
  uint16_t r = (color >> 11) & 0x1F;
  uint16_t g = (color >> 5) & 0x3F;
  uint16_t b = color & 0x1F;
  return ((r / 4) << 11) | ((g / 4) << 5) | (b / 4);
}

void show(const char* text, uint16_t accentColor, uint16_t bgColor) {
  strncpy(s_text, text, sizeof(s_text) - 1);
  s_text[sizeof(s_text) - 1] = '\0';
  s_accentColor = accentColor;
  s_bgColor = (bgColor == 0) ? darken(accentColor) : bgColor;
  s_visible = true;
  s_needsClear = false;
  renderAndFlush();
}

void showAuto(const char* text, uint16_t accentColor) {
  show(text, accentColor, darken(accentColor));
}

void hide() {
  s_visible = false;
  s_text[0] = '\0';
  clearFromScreen();
}

void update() {
  if (s_visible) renderAndFlush();
}

} // namespace TopChip
