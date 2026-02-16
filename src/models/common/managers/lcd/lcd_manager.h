#ifndef LCD_MANAGER_H
#define LCD_MANAGER_H

#include <Arduino.h>
#include "../../../model_config.h"

#ifdef HAS_LCD
class LGFX_Kidoo;
#include <JPEGDEC.h>
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
  static void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
  static void drawCircle(int16_t x, int16_t y, int16_t r, uint16_t color);
  static void fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color);

  /** Affiche un buffer RGB565 (ex: frame vidéo .bin) */
  static void pushImage(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* data);

  /** Affiche un buffer RGB565 via DMA si disponible (fluide pour animations .anim) */
  static void pushImageDMA(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* data);

  /** Attend la fin du transfert DMA en cours (optionnel, avant de réutiliser le buffer) */
  static void waitDMA();

  /** Change la rotation de l'écran (0=portrait 240x280, 1=landscape 280x240). Pour .anim utiliser 0. */
  static void setRotation(uint8_t r);

  // Dimensions
  static int16_t width();
  static int16_t height();

  // Rétroéclairage (si TFT_BLK_PIN défini dans la config du modèle)
  static void setBacklight(bool on);

  // Utilitaires
  static void printInfo();

  /** Test LCD : affiche rouge, puis bleu, puis vert (réinit écran avant pour fiabilité bus SPI partagé) */
  static void testLCD();

  /** Réinitialise l’écran (reset RST + init) pour repartir d’un état propre quand l’écran ne répond plus */
  static void reinitDisplay();

  /** À appeler en loop() : ré-init LCD une fois ~2,5 s après boot (corrige "après reboot pas d'affichage"). */
  static void tryDelayedReinit();

  /** Callback appelé après la re-init différée (ex. afficher "Kidoo Gotchi"). */
  static void setPostReinitCallback(void (*fn)());

  /** true pendant ~1,5 s après la re-init différée (ne pas dessiner l'animation par-dessus l'écran de démarrage). */
  static bool isStartupScreenVisible();

  /** Test FPS : animation frame par frame (rectangle rebondissant) pendant ~3 s, affiche les FPS */
  static void testFPS();

  /** Joue un .mjpeg vidéo depuis la SD (format serveur: landscape 280x240, 10 fps, MJPEG, pivoté avec transpose=1) */
  static void playMjpegFromSD(const char* path);

  /** Affiche une frame JPEG individuelle depuis un buffer mémoire */
  static bool displayJpegFrame(const uint8_t* jpegData, size_t jpegSize);

#ifdef HAS_LCD
  /** Callback JPEGDEC pour dessiner les pixels décodés sur l'écran */
  static int jpegDrawCallback(JPEGDRAW *pDraw);
#endif

  // Couleurs courantes (format RGB565)
  static const uint16_t COLOR_BLACK = 0x0000;
  static const uint16_t COLOR_WHITE = 0xFFFF;
  static const uint16_t COLOR_RED = 0xF800;
  static const uint16_t COLOR_GREEN = 0x07E0;
  static const uint16_t COLOR_BLUE = 0x001F;

private:
#ifdef HAS_LCD
  static LGFX_Kidoo* _tft;
  static int16_t _mjpegOffsetX;  // Offset horizontal pour centrer le MJPEG
  static int16_t _mjpegOffsetY;  // Offset vertical pour centrer le MJPEG
  static bool _dmaInitialized;   // initDMA() appelé au premier pushImageDMA (évite d’affecter le logo au boot)
  static void (*_postReinitCallback)();
  static unsigned long _startupScreenVisibleUntil;
#endif
  static bool _initialized;
  static bool _available;
};

#endif // LCD_MANAGER_H
