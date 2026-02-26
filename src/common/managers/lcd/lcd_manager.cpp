#include "lcd_manager.h"
#include "models/model_config.h"
#include "common/config/core_config.h"
#if defined(HAS_LCD) && defined(HAS_SD)
#include <SD.h>
#include <esp_heap_caps.h>
#endif

#ifdef HAS_LCD
#include "lcd_lovyangfx_config.hpp"
#include <JPEGDEC.h>

static LGFX_Kidoo _display;
static JPEGDEC _jpeg;
#endif

#ifdef HAS_LCD
LGFX_Kidoo* LCDManager::_tft = nullptr;
int16_t LCDManager::_mjpegOffsetX = -15;  // Offset horizontal pour centrer
int16_t LCDManager::_mjpegOffsetY = 20;  // Offset vertical pour centrer
bool LCDManager::_dmaInitialized = false;
void (*LCDManager::_postReinitCallback)() = nullptr;
unsigned long LCDManager::_startupScreenVisibleUntil = 0;
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
  // Mode landscape (280x240) avec rotation 90° - vidéos pivotées côté serveur
  int16_t w = 280;
  int16_t h = 240;
  uint8_t r = 1;  // rotation 1 = landscape 90° (280x240)
#endif
  Serial.printf("[LCD] Initialisation ecran ST7789 %dx%d rotation=%d (LovyanGFX)...\n", w, h, r);
  Serial.printf("[LCD] Pins: CS=%d, DC=%d, RST=%d, MOSI(SDA)=%d, SCK(SCL)=%d\n",
                TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN, TFT_MOSI_PIN, TFT_SCK_PIN);

  // Cold boot : laisser le bus SPI et la SD bien inactifs (après upload OK, après reboot souvent KO sans ce délai)
#if defined(HAS_SD)
  delay(700);
#endif

  _tft = &_display;
  if (!_tft) {
    Serial.println("[LCD] ERREUR: Pointeur TFT invalide");
    return false;
  }

  // Plusieurs tentatives avec reset complet (évite de devoir rebooter pour que l'écran s'affiche)
  const int maxAttempts = 6;
  bool initOk = false;
  pinMode(TFT_RST_PIN, OUTPUT);

  for (int attempt = 1; attempt <= maxAttempts; attempt++) {
    // Reset matériel complet
    digitalWrite(TFT_RST_PIN, HIGH);
    delay(30);
    digitalWrite(TFT_RST_PIN, LOW);
    delay(150);
    digitalWrite(TFT_RST_PIN, HIGH);
    delay(attempt == 1 ? 200 : 300);
    digitalWrite(TFT_RST_PIN, LOW);
    delay(150);
    digitalWrite(TFT_RST_PIN, HIGH);
    delay(250);

    if (_tft->init()) {
      initOk = true;
      if (attempt > 1) {
        Serial.printf("[LCD] Ecran OK a la tentative %d/%d\n", attempt, maxAttempts);
      }
      break;
    }
    Serial.printf("[LCD] Init echouee tentative %d/%d, reset et nouvel essai...\n", attempt, maxAttempts);
  }

  if (!initOk) {
    Serial.println("[LCD] ERREUR: Init LovyanGFX echouee apres toutes les tentatives");
    return false;
  }

  // Laisser le panneau se réveiller avant toute commande
  delay(120);
  _tft->setRotation(r);
  delay(80);

#ifdef TFT_BLK_PIN
  pinMode(TFT_BLK_PIN, OUTPUT);
#if defined(TFT_BLK_ACTIVE_LOW)
  digitalWrite(TFT_BLK_PIN, LOW);   // Rétroéclairage ON (module actif à LOW)
#else
  digitalWrite(TFT_BLK_PIN, HIGH);  // Rétroéclairage ON (module actif à HIGH)
#endif
  delay(20);
#endif

  _tft->fillScreen(COLOR_BLACK);
  delay(80);
  _tft->fillScreen(COLOR_BLACK);
  delay(50);

  _available = true;
  Serial.println("[LCD] Ecran initialise (LovyanGFX)");
  /* initDMA() est appelé au premier pushImageDMA() pour ne pas perturber le logo / texte au boot */

  // JPEGDEC s'initialise à la volée pour chaque frame
  Serial.println("[LCD] JPEGDEC pret");

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
void LCDManager::reinitDisplay() {
  if (!_tft) return;
  pinMode(TFT_RST_PIN, OUTPUT);
  digitalWrite(TFT_RST_PIN, LOW);
  delay(150);
  digitalWrite(TFT_RST_PIN, HIGH);
  delay(180);
  _tft->init();
  uint8_t r = (TFT_ROTATION & 3);
  _tft->setRotation(r);
  _tft->fillScreen(COLOR_BLACK);
  setBacklight(true);
}

void LCDManager::tryDelayedReinit() {
#ifdef HAS_LCD
  static bool done = false;
  if (done || !_available || !_tft) return;
  if (millis() < 2500) return;
  done = true;
  Serial.println("[LCD] Re-init differee (apres reboot)...");
  reinitDisplay();
  _startupScreenVisibleUntil = millis() + 1500;  // Garder l'écran de démarrage visible 1,5 s
  if (_postReinitCallback) _postReinitCallback();
#endif
}

void LCDManager::setPostReinitCallback(void (*fn)()) {
#ifdef HAS_LCD
  _postReinitCallback = fn;
#endif
}

bool LCDManager::isStartupScreenVisible() {
#ifdef HAS_LCD
  return _startupScreenVisibleUntil != 0 && millis() < _startupScreenVisibleUntil;
#else
  return false;
#endif
}

// Callback JPEGDEC : dessine un bloc de pixels décodés sur l'écran
int LCDManager::jpegDrawCallback(JPEGDRAW *pDraw) {
  if (!_tft || !pDraw) {
    Serial.println("[JPEG-CB] Callback appelé avec _tft ou pDraw null");
    return 0;
  }

  // Debug première fois
  static bool firstCall = true;
  if (firstCall) {
    firstCall = false;
  }

  // Appliquer les offsets horizontal et vertical pour centrer l'image
  _tft->pushImage(pDraw->x + _mjpegOffsetX, pDraw->y + _mjpegOffsetY, pDraw->iWidth, pDraw->iHeight, (uint16_t*)pDraw->pPixels);
  return 1;  // 1 = continuer le décodage
}

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

void LCDManager::pushImageDMA(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* data) {
  if (!_tft || !data) return;
#if defined(ESP32) || defined(ESP32S3)
  if (!_dmaInitialized) {
    _tft->initDMA();
    _dmaInitialized = true;
    delay(35);  // Laisse le DMA/panneau prêt après la première init (évite écran noir au démarrage)
  }
  _tft->pushImageDMA(x, y, w, h, data);
#else
  _tft->pushImage(x, y, w, h, data);
#endif
}

void LCDManager::waitDMA() {
  if (_tft) _tft->waitDMA();
}

void LCDManager::setRotation(uint8_t r) {
  if (_tft) _tft->setRotation(r);
}

int16_t LCDManager::width() {
  return _tft ? _tft->width() : 0;
}

int16_t LCDManager::height() {
  return _tft ? _tft->height() : 0;
}

void LCDManager::setBacklight(bool on) {
#ifdef TFT_BLK_PIN
#if defined(TFT_BLK_ACTIVE_LOW)
  digitalWrite(TFT_BLK_PIN, on ? LOW : HIGH);
#else
  digitalWrite(TFT_BLK_PIN, on ? HIGH : LOW);
#endif
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
void LCDManager::pushImageDMA(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* data) { (void)x; (void)y; (void)w; (void)h; (void)data; }
void LCDManager::waitDMA() { }
void LCDManager::setRotation(uint8_t r) { (void)r; }
void LCDManager::reinitDisplay() { }
void LCDManager::tryDelayedReinit() { }
void LCDManager::setPostReinitCallback(void (*fn)()) { (void)fn; }
bool LCDManager::isStartupScreenVisible() { return false; }
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
  waitDMA();
  setBacklight(true);
  delay(80);
  reinitDisplay();
  delay(50);
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

  const int TARGET_FPS = 15;  // Augmenté de 10 à 15 FPS pour plus de fluidité
  const uint32_t FRAME_MS = 1000 / TARGET_FPS;
  const size_t FRAME_BUF_SIZE = 131072;  // 128 KB - marge pour JPEG qualité 5 (yuv420p)

  char filePath[256];  // Augmenté de 64 à 256 pour supporter les chemins longs avec UUIDs
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

    // Afficher les infos de la première frame pour debug
    if (frameCount == 0) {
      Serial.printf("[LCD-PLAY] Premiere frame: %u bytes (SOI@%u, EOI@%u)\n",
                    (unsigned)frameLen, (unsigned)soiPos, (unsigned)eoiPos);
      Serial.printf("[LCD-PLAY] Header JPEG: %02X %02X %02X %02X %02X %02X %02X %02X\n",
                    frameBuf[soiPos], frameBuf[soiPos+1], frameBuf[soiPos+2], frameBuf[soiPos+3],
                    frameBuf[soiPos+4], frameBuf[soiPos+5], frameBuf[soiPos+6], frameBuf[soiPos+7]);
    }

    // Décoder avec JPEGDEC (supporte tous les formats JPEG standards)
    int result = _jpeg.openRAM(frameBuf + soiPos, (int)frameLen, jpegDrawCallback);
    bool success = (result == 1);  // 1 = succès

    int w = 0, h = 0;
    if (success) {
      w = _jpeg.getWidth();
      h = _jpeg.getHeight();
      // Configurer le format de pixel en Little Endian pour LovyanGFX pushImage()
      // RGB565_LITTLE_ENDIAN = 1 (par défaut JPEGDEC sort en Big Endian)
      _jpeg.setPixelType(1);
      // 0 = pas de scaling (1:1), pas de flags spéciaux
      result = _jpeg.decode(0, 0, 0);
      success = (result == 1);
      _jpeg.close();
    }

    if (frameCount == 0 || !success) {
      Serial.printf("[LCD-PLAY] Frame %u: %s (taille=%u bytes, dim=%dx%d, code=%d)\n",
                    (unsigned)frameCount, success ? "OK" : "ECHEC", (unsigned)frameLen, w, h, result);
    }

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

bool LCDManager::displayJpegFrame(const uint8_t* jpegData, size_t jpegSize) {
#if defined(HAS_LCD)
  if (!_available || !_tft) {
    return false;
  }

  // Décoder le JPEG avec JPEGDEC
  int result = _jpeg.openRAM((uint8_t*)jpegData, (int)jpegSize, jpegDrawCallback);
  if (result != 1) {
    Serial.printf("[LCD] ERREUR: openRAM failed (result=%d, size=%d)\n", result, jpegSize);
    return false;
  }

  // Configurer le format de pixel APRÈS openRAM (important!)
  // 0=RGB565_BE, 1=RGB565_LE, 2=RGB888
  _jpeg.setPixelType(1);  // Little Endian

  // Décoder et afficher
  result = _jpeg.decode(0, 0, 0);
  _jpeg.close();

  if (result != 1) {
    Serial.printf("[LCD] ERREUR: decode failed (result=%d)\n", result);
    return false;
  }

  return true;
#else
  (void)jpegData;
  (void)jpegSize;
  return false;
#endif
}
