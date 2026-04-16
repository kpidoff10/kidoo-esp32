#ifndef GOTCHI_SPRITE_ASSET_H
#define GOTCHI_SPRITE_ASSET_H

#include <cstdint>

// Décrit un sprite stocké en PROGMEM.
//
// Deux modes :
//   - alpha-only (rgb565 == nullptr) : 1 byte/pixel = alpha. La couleur est
//     fournie au runtime → permet de coloriser dynamiquement (étoile jaune,
//     bleue, blanche selon le contexte). Format produit par png_to_sprite.py.
//
//   - rgba (rgb565 != nullptr) : 2 bytes/pixel pour la couleur native + 1
//     byte/pixel pour l'alpha. Le sprite garde ses couleurs d'origine, idéal
//     pour les emojis multi-couleur. Format produit par png_to_sprite_rgba.py.
struct SpriteAsset {
  uint16_t width;
  uint16_t height;
  const uint8_t* alpha;    // PROGMEM, w*h bytes, toujours présent
  const uint16_t* rgb565;  // PROGMEM, w*h words, nullptr si mono-alpha
};

#endif // GOTCHI_SPRITE_ASSET_H
