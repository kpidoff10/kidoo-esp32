#ifndef TOP_CHIP_H
#define TOP_CHIP_H

#include <cstdint>

// TopChip — composant réutilisable pour afficher un chip (pill/badge)
// en haut de l'écran du Gotchi. Utilise son propre mini-framebuffer
// et flush directement via Arduino_GFX (pas de conflit avec le top buffer).
//
// Usage:
//   TopChip::show("38.5^C", 0xF800);   // texte rouge, fond auto-darken
//   TopChip::show("OK", 0x07E0, 0x0320); // texte vert, fond custom
//   TopChip::hide();
//
// Caractères supportés: 0-9, A-Z, a-z (→ majuscule), '.', '^'(=°), ' ', ':', '-', '%'

namespace TopChip {

// Afficher le chip. bgColor=0 → fond = version sombre de accentColor.
void show(const char* text, uint16_t accentColor, uint16_t bgColor = 0);

// Masquer le chip et effacer de l'écran.
void hide();

// Re-render le chip (appeler dans onUpdate si la valeur change).
void update();

// Assombrir une couleur RGB565.
uint16_t darken(uint16_t color);

} // namespace TopChip

#endif // TOP_CHIP_H
