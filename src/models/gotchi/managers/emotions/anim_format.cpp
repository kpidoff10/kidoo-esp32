/**
 * Implémentation du format .anim (palette 8-bit + RLE horizontal)
 */

#include "anim_format.h"
#include <SD.h>

/** Swap des octets (endianness) : palette en LE, pushImageDMA peut envoyer tel quel et le ST7789 interprète en BE. */
static inline uint16_t rgb565_swap_bytes(uint16_t c) {
  return (uint16_t)((c >> 8) | (c << 8));
}

bool loadAnimHeader(fs::File& f, AnimHeader& h) {
  if (f.available() < (int)ANIM_HEADER_SIZE) {
    return false;
  }
  f.read((uint8_t*)&h, ANIM_HEADER_SIZE);
  if (memcmp(h.magic, ANIM_MAGIC, 4) != 0) {
    return false;
  }
  if (h.version != ANIM_VERSION) {
    return false;
  }
  // 0 = 256 couleurs (uint8 ne peut pas stocker 256)
  if (h.palette_size > 256) {
    return false;
  }
  return true;
}

bool loadPalette(fs::File& f, uint16_t palette[256], uint16_t palette_size) {
  const size_t bytes = (size_t)palette_size * 2;
  if (f.available() < (int)bytes) {
    return false;
  }
  for (uint16_t i = 0; i < palette_size; i++) {
    uint16_t c;
    if (f.read((uint8_t*)&c, 2) != 2) {
      return false;
    }
    palette[i] = c;
  }
  return true;
}

bool decodeRLEFrame(const uint8_t* rle_data, uint32_t rle_size,
                   uint16_t* rgb_out,
                   const uint16_t palette[256], uint16_t palette_size,
                   int width, int height,
                   bool index0_transparent, uint16_t bg_color) {
  const int pixels_per_line = width;
  const int total_pixels = width * height;
  int x = 0, y = 0;
  uint32_t i = 0;

  while (y < height && i + 1 < rle_size) {
    uint8_t run_length = rle_data[i];
    uint8_t color_index = rle_data[i + 1];
    i += 2;
    if (run_length == 0) run_length = 1;  /* robustesse : évite boucle vide */

    uint16_t color;
    if (index0_transparent && color_index == 0) {
      color = rgb565_swap_bytes(bg_color);
    } else {
      if (color_index >= palette_size) {
        color_index = palette_size - 1;
      }
      color = palette[color_index];
      /* Aligner ordre octets pour pushImageDMA / ST7789 → swap pour que rose reste rose sur l’écran */
      color = rgb565_swap_bytes(color);
    }

    for (uint8_t k = 0; k < run_length && y < height; k++) {
      if (x < pixels_per_line) {
        rgb_out[y * width + x] = color;
      }
      x++;
      if (x >= pixels_per_line) {
        x = 0;
        y++;
      }
    }
  }

  /* Remplir le reste avec bg si ligne incomplète ou fin de flux */
  const uint16_t bg_swapped = rgb565_swap_bytes(bg_color);
  while (y < height) {
    while (x < pixels_per_line) {
      rgb_out[y * width + x] = bg_swapped;
      x++;
    }
    x = 0;
    y++;
  }
  return true;
}
