#include "lcd_manager.h"
#include "../../../model_config.h"
#include "../../config/core_config.h"

#ifdef HAS_LCD
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#endif

#ifdef HAS_LCD
Adafruit_ST7789* LCDManager::_tft = nullptr;
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
  Serial.printf("[LCD] Initialisation ecran ST7789 %dx%d rotation=%d...\n", w, h, r);
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

  _tft = new Adafruit_ST7789(TFT_CS_PIN, TFT_DC_PIN, TFT_MOSI_PIN, TFT_SCK_PIN, TFT_RST_PIN);

  if (!_tft) {
    Serial.println("[LCD] ERREUR: Allocation memoire echouee");
    return false;
  }

  _tft->init(w, h);
  _tft->setRotation(r);
  #if defined(ESP32)
  _tft->setSPISpeed(20000000);  // 20 MHz pour limiter lignes / pixels blancs (après init)
  #endif
  delay(50);
  _tft->fillScreen(COLOR_BLACK);
  delay(10);
  _tft->fillScreen(COLOR_BLACK);

#ifdef TFT_BLK_PIN
  pinMode(TFT_BLK_PIN, OUTPUT);
  digitalWrite(TFT_BLK_PIN, HIGH);  // Rétroéclairage ON (module: LOW = éteint)
#endif

  _available = true;
  Serial.println("[LCD] Ecran initialise");

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
  Serial.println("[LCD] Modele: ST7789 240x280 SPI");
  Serial.printf("[LCD] Dimensions: %dx%d\n", width(), height());
  Serial.println("==============================");
#else
  Serial.println("[LCD] LCD non active (HAS_LCD non defini)");
#endif
}
