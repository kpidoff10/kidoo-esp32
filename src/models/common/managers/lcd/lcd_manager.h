#ifndef LCD_MANAGER_H
#define LCD_MANAGER_H

#include <Arduino.h>
#include "../../../model_config.h"

#ifdef HAS_LCD
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#endif

/**
 * Gestionnaire LCD IPS - ST7789 - 240x280 - SPI
 *
 * Utilisé pour afficher l'état du Gotchi (Tamagotchi) et autres informations.
 * Activé via HAS_LCD dans la config du modèle.
 */

class LCDManager {
public:
  static bool init();
  static bool isAvailable();
  static bool isInitialized();

  // Affichage basique
  static void fillScreen(uint16_t color);
  static void setCursor(int16_t x, int16_t y);
  static void setTextColor(uint16_t color);
  static void setTextSize(uint8_t s);
  static void print(const char* text);
  static void println(const char* text);

  // Dessin
  static void drawPixel(int16_t x, int16_t y, uint16_t color);
  static void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  static void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  static void drawCircle(int16_t x, int16_t y, int16_t r, uint16_t color);

  // Dimensions
  static int16_t width();
  static int16_t height();

  // Utilitaires
  static void printInfo();

  // Couleurs courantes (format RGB565)
  static const uint16_t COLOR_BLACK = 0x0000;
  static const uint16_t COLOR_WHITE = 0xFFFF;
  static const uint16_t COLOR_RED = 0xF800;
  static const uint16_t COLOR_GREEN = 0x07E0;
  static const uint16_t COLOR_BLUE = 0x001F;

private:
#ifdef HAS_LCD
  static Adafruit_ST7789* _tft;
#endif
  static bool _initialized;
  static bool _available;
};

#endif // LCD_MANAGER_H
