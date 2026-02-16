/**
 * Format .anim : animation lossless palette indexée 8-bit + RLE horizontal
 * Structure binaire little-endian.
 *
 * Header (14 octets) : magic "ANIM", version, num_frames, width, height, palette_size
 * Palette : palette_size × 2 octets (RGB565)
 * Par frame : uint32_t rle_data_size puis uint8_t rle_data[rle_data_size]
 * RLE : paires (run_length 1..255, color_index), par ligne, 240 px/ligne
 */

#ifndef ANIM_FORMAT_H
#define ANIM_FORMAT_H

#include <Arduino.h>
#include <FS.h>  // fs::File (SD / LittleFS)

#define ANIM_MAGIC "ANIM"
#define ANIM_VERSION 1
#define ANIM_HEADER_SIZE 14

/** Header fixe du fichier .anim (14 bytes, little-endian) */
struct AnimHeader {
  char magic[4];       // "ANIM"
  uint8_t version;     // 1
  uint16_t num_frames;
  uint16_t width;     // 240
  uint16_t height;    // 280
  uint8_t palette_size;  // 1..256
  uint8_t reserved[2];  // align 14 bytes total
};

/**
 * Lit le header .anim depuis le fichier déjà ouvert.
 * Le fichier doit être positionné au début (après open).
 * @return true si magic/version valides
 */
bool loadAnimHeader(fs::File& f, AnimHeader& h);

/**
 * Charge la palette globale (palette_size × 2 bytes RGB565).
 * Le fichier doit être positionné juste après le header.
 * @param palette tableau d'au moins 256 uint16_t
 */
bool loadPalette(fs::File& f, uint16_t palette[256], uint16_t palette_size);

/**
 * Décode une frame RLE dans un buffer RGB565.
 * RLE horizontal : par ligne, paires (run_length, color_index). 240 pixels par ligne.
 * Si index 0 = transparent : on remplit avec bg_color (ex. 0x0000).
 *
 * @param rle_data buffer des octets RLE (pas le uint32 size)
 * @param rle_size nombre d'octets dans rle_data
 * @param rgb_out buffer RGB565 sortie (width * height pixels)
 * @param palette palette RGB565 (au moins palette_size entrées)
 * @param palette_size nombre de couleurs
 * @param width 240
 * @param height 280
 * @param index0_transparent si true, l'index 0 est affiché avec bg_color
 * @param bg_color couleur de fond pour index 0 (souvent 0x0000)
 * @return true si décodage OK (nombre de pixels écrits = width*height)
 */
bool decodeRLEFrame(const uint8_t* rle_data, uint32_t rle_size,
                   uint16_t* rgb_out,
                   const uint16_t palette[256], uint16_t palette_size,
                   int width, int height,
                   bool index0_transparent, uint16_t bg_color);

#endif // ANIM_FORMAT_H
