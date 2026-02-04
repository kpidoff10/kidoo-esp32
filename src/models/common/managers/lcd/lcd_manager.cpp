#include "lcd_manager.h"
#include "../../../model_config.h"
#include "../../config/core_config.h"
#if defined(HAS_LCD) && defined(HAS_SD)
#include <SD.h>
#include <esp_heap_caps.h>
#endif

#ifdef HAS_LCD
#include "lcd_lovyangfx_config.hpp"

static LGFX_Kidoo _display;
#endif

#ifdef HAS_LCD
LGFX_Kidoo* LCDManager::_tft = nullptr;
#endif
bool LCDManager::_initialized = false;
bool LCDManager::_available = false;

bool LCDManager::init() {
#ifndef HAS_LCD
  _initialized = true;
  _available = false;
  return true;
#else
  if (_initialized) {
    return _available;
  }

  _initialized = true;
  _available = false;

#ifdef TFT_WIDTH
  int16_t w = TFT_WIDTH;
  int16_t h = TFT_HEIGHT;
  uint8_t r = (TFT_ROTATION & 3);
#else
  int16_t w = 240;
  int16_t h = 280;
  uint8_t r = 2;
#endif
  Serial.printf("[LCD] Initialisation ecran ST7789 %dx%d rotation=%d (LovyanGFX)...\n", w, h, r);
  Serial.printf("[LCD] Pins: CS=%d, DC=%d, RST=%d, MOSI(SDA)=%d, SCK(SCL)=%d\n",
                TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN, TFT_MOSI_PIN, TFT_SCK_PIN);

  // Reset matériel propre pour éviter pixels morts et lignes fantômes
  pinMode(TFT_RST_PIN, OUTPUT);
  digitalWrite(TFT_RST_PIN, HIGH);
  delay(20);
  digitalWrite(TFT_RST_PIN, LOW);
  delay(100);
  digitalWrite(TFT_RST_PIN, HIGH);
  delay(120);

  _tft = &_display;

  if (!_tft) {
    Serial.println("[LCD] ERREUR: Pointeur TFT invalide");
    return false;
  }

  if (!_tft->init()) {
    Serial.println("[LCD] ERREUR: Init LovyanGFX echouee");
    return false;
  }

  _tft->setRotation(r);
  delay(50);
  _tft->fillScreen(COLOR_BLACK);
  delay(10);
  _tft->fillScreen(COLOR_BLACK);

#ifdef TFT_BLK_PIN
  pinMode(TFT_BLK_PIN, OUTPUT);
  digitalWrite(TFT_BLK_PIN, HIGH);  // Rétroéclairage ON (module: LOW = éteint)
#endif

  _available = true;
  Serial.println("[LCD] Ecran initialise (LovyanGFX)");

  return _available;
#endif
}

bool LCDManager::isAvailable() {
  return _initialized && _available;
}

bool LCDManager::isInitialized() {
  return _initialized;
}

#ifdef HAS_LCD
void LCDManager::fillScreen(uint16_t color) {
  if (_tft) _tft->fillScreen(color);
}

void LCDManager::setCursor(int16_t x, int16_t y) {
  if (_tft) _tft->setCursor(x, y);
}

void LCDManager::setTextColor(uint16_t color) {
  if (_tft) _tft->setTextColor(color);
}

void LCDManager::setTextSize(uint8_t s) {
  if (_tft) _tft->setTextSize(s);
}

void LCDManager::print(const char* text) {
  if (_tft) _tft->print(text);
}

void LCDManager::println(const char* text) {
  if (_tft) _tft->println(text);
}

void LCDManager::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if (_tft) _tft->drawPixel(x, y, color);
}

void LCDManager::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (_tft) _tft->drawRect(x, y, w, h, color);
}

void LCDManager::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (_tft) _tft->fillRect(x, y, w, h, color);
}

void LCDManager::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
  if (_tft) _tft->drawLine(x0, y0, x1, y1, color);
}

void LCDManager::drawCircle(int16_t x, int16_t y, int16_t r, uint16_t color) {
  if (_tft) _tft->drawCircle(x, y, r, color);
}

void LCDManager::fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color) {
  if (_tft) _tft->fillCircle(x, y, r, color);
}

void LCDManager::pushImage(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* data) {
  if (_tft && data) _tft->pushImage(x, y, w, h, data);
}

int16_t LCDManager::width() {
  return _tft ? _tft->width() : 0;
}

int16_t LCDManager::height() {
  return _tft ? _tft->height() : 0;
}

void LCDManager::setBacklight(bool on) {
#ifdef TFT_BLK_PIN
  digitalWrite(TFT_BLK_PIN, on ? HIGH : LOW);
#endif
}
#else
void LCDManager::fillScreen(uint16_t color) { (void)color; }
void LCDManager::setCursor(int16_t x, int16_t y) { (void)x; (void)y; }
void LCDManager::setTextColor(uint16_t color) { (void)color; }
void LCDManager::setTextSize(uint8_t s) { (void)s; }
void LCDManager::print(const char* text) { (void)text; }
void LCDManager::println(const char* text) { (void)text; }
void LCDManager::drawPixel(int16_t x, int16_t y, uint16_t color) { (void)x; (void)y; (void)color; }
void LCDManager::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) { (void)x; (void)y; (void)w; (void)h; (void)color; }
void LCDManager::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) { (void)x; (void)y; (void)w; (void)h; (void)color; }
void LCDManager::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) { (void)x0; (void)y0; (void)x1; (void)y1; (void)color; }
void LCDManager::drawCircle(int16_t x, int16_t y, int16_t r, uint16_t color) { (void)x; (void)y; (void)r; (void)color; }
void LCDManager::fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color) { (void)x; (void)y; (void)r; (void)color; }
void LCDManager::pushImage(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* data) { (void)x; (void)y; (void)w; (void)h; (void)data; }
int16_t LCDManager::width() { return 0; }
int16_t LCDManager::height() { return 0; }
void LCDManager::setBacklight(bool on) { (void)on; }
#endif

void LCDManager::printInfo() {
#ifdef HAS_LCD
  if (!_available) {
    Serial.println("[LCD] Ecran non disponible");
    return;
  }
  Serial.println("");
  Serial.println("========== Etat LCD ==========");
  Serial.println("[LCD] Modele: ST7789 240x280 SPI (LovyanGFX)");
  Serial.printf("[LCD] Dimensions: %dx%d\n", width(), height());
  Serial.println("==============================");
#else
  Serial.println("[LCD] LCD non active (HAS_LCD non defini)");
#endif
}

void LCDManager::testLCD() {
#ifdef HAS_LCD
  if (!isAvailable()) {
    Serial.println("[LCD-TEST] LCD non disponible");
    return;
  }
  Serial.println("[LCD-TEST] Rouge...");
  fillScreen(COLOR_RED);
  delay(1500);
  Serial.println("[LCD-TEST] Bleu...");
  fillScreen(COLOR_BLUE);
  delay(1500);
  Serial.println("[LCD-TEST] Vert...");
  fillScreen(COLOR_GREEN);
  delay(1500);
  fillScreen(COLOR_BLACK);
  Serial.println("[LCD-TEST] Termine");
#else
  (void)0;  // Stub quand HAS_LCD non défini (cmdLCDTest gère l'affichage)
#endif
}

void LCDManager::testFPS() {
#ifdef HAS_LCD
  if (!isAvailable()) {
    Serial.println("[LCD-FPS] LCD non disponible");
    return;
  }
  const int W = 40;
  const int H = 40;
  int x = 0;
  int y = 0;
  int dx = 4;
  int dy = 3;
  uint32_t frameCount = 0;
  const uint32_t durationMs = 3000;
  uint32_t startMs = millis();

  Serial.println("[LCD-FPS] Animation 3 secondes (rectangle rebondissant)...");

  while ((millis() - startMs) < durationMs) {
    fillScreen(COLOR_BLACK);
    fillRect(x, y, W, H, COLOR_WHITE);
    x += dx;
    y += dy;
    if (x <= 0) { x = 0; dx = -dx; }
    if (y <= 0) { y = 0; dy = -dy; }
    if (x + W >= width()) { x = width() - W; dx = -dx; }
    if (y + H >= height()) { y = height() - H; dy = -dy; }
    frameCount++;
  }

  uint32_t elapsed = millis() - startMs;
  float fps = (elapsed > 0) ? (1000.0f * frameCount / elapsed) : 0.0f;

  fillScreen(COLOR_BLACK);
  Serial.printf("[LCD-FPS] %u frames en %u ms = %.1f FPS\n", (unsigned)frameCount, (unsigned)elapsed, fps);
#else
  (void)0;
#endif
}

void LCDManager::playMjpegFromSD(const char* path) {
#if defined(HAS_LCD) && defined(HAS_SD)
  if (!isAvailable()) {
    Serial.println("[LCD-PLAY] LCD non disponible");
    return;
  }
  if (!path || path[0] == '\0') {
    Serial.println("[LCD-PLAY] Usage: lcd-play-mjpeg <chemin> (ex: /video.mjpeg ou /clips/video.mjpeg)");
    return;
  }

  const int TARGET_FPS = 10;
  const uint32_t FRAME_MS = 1000 / TARGET_FPS;
  const size_t FRAME_BUF_SIZE = 131072;  // 128 KB - marge pour JPEG qualité élevée (q:v 3)

  char filePath[64];
  if (path[0] != '/') {
    filePath[0] = '/';
    strncpy(filePath + 1, path, sizeof(filePath) - 2);
    filePath[sizeof(filePath) - 1] = '\0';
  } else {
    strncpy(filePath, path, sizeof(filePath) - 1);
    filePath[sizeof(filePath) - 1] = '\0';
  }

  File f = SD.open(filePath, FILE_READ);
  if (!f) {
    Serial.printf("[LCD-PLAY] Fichier introuvable: %s\n", filePath);
    return;
  }

  uint8_t* frameBuf = (uint8_t*)heap_caps_malloc(FRAME_BUF_SIZE, MALLOC_CAP_SPIRAM);
  if (!frameBuf) {
    frameBuf = (uint8_t*)malloc(FRAME_BUF_SIZE);
  }
  if (!frameBuf) {
    f.close();
    Serial.println("[LCD-PLAY] Erreur allocation memoire (128 KB)");
    return;
  }

  Serial.printf("[LCD-PLAY] Lecture MJPEG en streaming...\n");

  uint32_t frameCount = 0;
  size_t bufLen = 0;
  const size_t CHUNK = 4096;

  while (true) {
    if (bufLen < FRAME_BUF_SIZE) {
      size_t toRead = FRAME_BUF_SIZE - bufLen;
      if (toRead > CHUNK) toRead = CHUNK;
      int n = f.read(frameBuf + bufLen, toRead);
      if (n <= 0) break;
      bufLen += (size_t)n;
    }

    if (bufLen < 2) break;

    size_t soiPos = (size_t)-1;
    for (size_t i = 0; i + 2 <= bufLen; i++) {
      if (frameBuf[i] == 0xFF && frameBuf[i + 1] == 0xD8) {
        soiPos = i;
        break;
      }
    }
    if (soiPos == (size_t)-1) {
      if (bufLen >= 1 && frameBuf[bufLen - 1] == 0xFF) {
        frameBuf[0] = frameBuf[bufLen - 1];
        bufLen = 1;
      } else {
        bufLen = 0;
      }
      continue;
    }

    size_t eoiPos = (size_t)-1;
    for (size_t j = soiPos + 2; j + 2 <= bufLen; j++) {
      if (frameBuf[j] == 0xFF && frameBuf[j + 1] == 0xD9) {
        eoiPos = j + 2;
        break;
      }
    }
    if (eoiPos == (size_t)-1) {
      if (bufLen >= FRAME_BUF_SIZE) {
        memmove(frameBuf, frameBuf + soiPos, bufLen - soiPos);
        bufLen -= soiPos;
      }
      continue;
    }

    size_t frameLen = eoiPos - soiPos;
    uint32_t frameStart = millis();
    _tft->drawJpg(frameBuf + soiPos, (uint32_t)frameLen, 0, 0, 240, 280, 0, 0, 1.0f, 1.0f);
    frameCount++;

    memmove(frameBuf, frameBuf + eoiPos, bufLen - eoiPos);
    bufLen -= eoiPos;

    uint32_t elapsed = millis() - frameStart;
    if (elapsed < FRAME_MS) {
      delay(FRAME_MS - elapsed);
    }
  }

  free(frameBuf);
  f.close();

  delay(50);
  Serial.printf("[LCD-PLAY] Termine: %u frames\n", (unsigned)frameCount);
#else
  (void)path;
  Serial.println("[LCD-PLAY] LCD ou SD non disponible");
#endif
}
