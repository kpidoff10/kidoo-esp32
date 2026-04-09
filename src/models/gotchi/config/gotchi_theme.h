#ifndef GOTCHI_THEME_H
#define GOTCHI_THEME_H

#include <cstdint>

namespace GotchiTheme {

// Presets de couleur
enum class Preset {
  Boy,    // Cyan/bleu
  Girl,   // Rose/magenta
  Green,  // Vert neon
  Gold,   // Or/jaune
  Red,    // Rouge
  White,  // Blanc pur
};

// Couleurs RGB565
struct Colors {
  uint16_t eye;       // Yeux + bouche contour
  uint16_t inner;     // Interieur bouche
  uint16_t tongue;    // Langue
  uint16_t overlay;   // Overlay (Zzz etc)
};

void setPreset(Preset p);
void setPresetByName(const char* name);  // "boy", "girl", "green", etc.
void setCustomColor(uint16_t rgb565);

const Colors& getColors();
Preset getCurrentPreset();
const char* getPresetName();

void loadFromConfig();  // Charger depuis SDConfig
void saveToConfig();    // Sauvegarder vers SDConfig

} // namespace GotchiTheme

#endif
