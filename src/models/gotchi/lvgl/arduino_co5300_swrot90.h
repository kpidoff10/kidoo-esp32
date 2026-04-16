#pragma once

/**
 * Arduino_CO5300_SwRot90
 * ----------------------
 * Sous-classe de Arduino_CO5300 qui applique une rotation logicielle de 90°
 * (sens horaire dans l'espace ecran) a tous les appels graphiques.
 *
 * Pourquoi : le controleur CO5300 ne supporte pas la rotation hardware
 * (cf. Arduino_CO5300.cpp:28 "CO5300 does not support rotation"). Son
 * setRotation() ne fait que des flips X/Y via MADCTL, pas de swap-XY.
 *
 * Approche : on override les 5 primitives virtuelles d'Arduino_GFX/TFT.
 * Toutes les methodes high-level (drawPixel, fillRect, fillCircle,
 * fillRoundRect, drawLine, drawCircle, fillScreen) funnel a travers ces
 * primitives, donc une seule sous-classe couvre 100% du dessin sans
 * toucher face_renderer, behavior_objects, view_*, draw_arc ni LVGL flush.
 *
 * Formule de rotation (W = H = 466, ecran carre) :
 *   logique (lx, ly) -> physique (W-1-ly, lx)
 *
 * Buffer de rotation : alloue lazy en PSRAM, max ~230 KB pour le push
 * frame face_renderer (426x270x2 bytes).
 */

#include <Arduino_GFX_Library.h>
#include <esp_heap_caps.h>
#include <stddef.h>
#include <stdint.h>

class Arduino_CO5300_SwRot90 : public Arduino_CO5300 {
public:
  using Arduino_CO5300::Arduino_CO5300;

  void writePixelPreclipped(int16_t lx, int16_t ly, uint16_t color) override {
    Arduino_CO5300::writePixelPreclipped(W - 1 - ly, lx, color);
  }

  void writeFillRectPreclipped(int16_t lx, int16_t ly, int16_t w, int16_t h, uint16_t color) override {
    // Rect logique (lx, ly, w, h) -> rect physique (W-ly-h, lx, h, w)
    Arduino_CO5300::writeFillRectPreclipped(W - ly - h, lx, h, w, color);
  }

  void writeFastVLine(int16_t lx, int16_t ly, int16_t h, uint16_t color) override {
    // V line logique (constant lx, ly..ly+h-1) devient H line physique
    // partant de (W-ly-h, lx) sur h pixels
    Arduino_CO5300::writeFastHLine(W - ly - h, lx, h, color);
  }

  void writeFastHLine(int16_t lx, int16_t ly, int16_t w, uint16_t color) override {
    // H line logique (lx..lx+w-1, constant ly) devient V line physique
    // partant de (W-1-ly, lx) sur w pixels
    Arduino_CO5300::writeFastVLine(W - 1 - ly, lx, w, color);
  }

  void draw16bitRGBBitmap(int16_t lx, int16_t ly, uint16_t *bitmap, int16_t w, int16_t h) override {
    if (!bitmap || w <= 0 || h <= 0) return;
    ensureRotBuf((size_t)w * (size_t)h);
    if (!rotBuf) return;

    // Rotation 90° CW : src(i, j) -> dest(h-1-j, i)
    // dest stocke h colonnes x w lignes en row-major (stride = h)
    for (int16_t j = 0; j < h; j++) {
      const uint16_t *srcRow = bitmap + (size_t)j * (size_t)w;
      int16_t destCol = h - 1 - j;
      for (int16_t i = 0; i < w; i++) {
        rotBuf[(size_t)i * (size_t)h + (size_t)destCol] = srcRow[i];
      }
    }

    int16_t px = W - ly - h;
    int16_t py = lx;
    Arduino_CO5300::draw16bitRGBBitmap(px, py, rotBuf, h, w);
  }

  void draw16bitRGBBitmap(int16_t lx, int16_t ly, const uint16_t bitmap[], int16_t w, int16_t h) override {
    // Belt-and-braces : forward vers la version non-const
    draw16bitRGBBitmap(lx, ly, const_cast<uint16_t *>(bitmap), w, h);
  }

private:
  static constexpr int16_t W = 466;  // GOTCHI_LCD_WIDTH = HEIGHT (ecran carre)

  uint16_t *rotBuf = nullptr;
  size_t    rotBufPixels = 0;

  void ensureRotBuf(size_t needed) {
    if (needed <= rotBufPixels) return;
    if (rotBuf) {
      heap_caps_free(rotBuf);
      rotBuf = nullptr;
    }
    size_t bytes = needed * sizeof(uint16_t);
    rotBuf = (uint16_t *)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    if (!rotBuf) {
      rotBuf = (uint16_t *)heap_caps_malloc(bytes, MALLOC_CAP_DEFAULT);
    }
    rotBufPixels = rotBuf ? needed : 0;
  }
};
