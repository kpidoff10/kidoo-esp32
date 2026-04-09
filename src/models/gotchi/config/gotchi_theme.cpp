#include "gotchi_theme.h"
#include "common/managers/sd/sd_manager.h"
#include <cstring>
#include <Arduino.h>

namespace {

GotchiTheme::Colors s_colors;
GotchiTheme::Preset s_preset = GotchiTheme::Preset::Boy;

void applyPreset(GotchiTheme::Preset p) {
  using P = GotchiTheme::Preset;
  switch (p) {
    case P::Boy:
      s_colors.eye     = 0x073F;  // Cyan ~00E5FF
      s_colors.inner   = 0x1928;  // Bleu fonce
      s_colors.tongue  = 0xF890;  // Rose
      s_colors.overlay = 0x073F;
      break;
    case P::Girl:
      s_colors.eye     = 0xF81F;  // Magenta ~FF00FF
      s_colors.inner   = 0x4010;  // Violet fonce
      s_colors.tongue  = 0xFE19;  // Rose clair
      s_colors.overlay = 0xF81F;
      break;
    case P::Green:
      s_colors.eye     = 0x07E0;  // Vert ~00FF00
      s_colors.inner   = 0x0320;  // Vert fonce
      s_colors.tongue  = 0xF890;  // Rose
      s_colors.overlay = 0x07E0;
      break;
    case P::Gold:
      s_colors.eye     = 0xFE20;  // Or ~FFD000
      s_colors.inner   = 0x4200;  // Brun fonce
      s_colors.tongue  = 0xF890;  // Rose
      s_colors.overlay = 0xFE20;
      break;
    case P::Red:
      s_colors.eye     = 0xF800;  // Rouge ~FF0000
      s_colors.inner   = 0x4000;  // Rouge fonce
      s_colors.tongue  = 0xFE19;  // Rose clair
      s_colors.overlay = 0xF800;
      break;
    case P::White:
      s_colors.eye     = 0xFFFF;  // Blanc
      s_colors.inner   = 0x4208;  // Gris fonce
      s_colors.tongue  = 0xF890;  // Rose
      s_colors.overlay = 0xFFFF;
      break;
  }
}

struct PresetEntry {
  const char* name;
  GotchiTheme::Preset preset;
};

constexpr PresetEntry PRESETS[] = {
  {"boy",   GotchiTheme::Preset::Boy},
  {"girl",  GotchiTheme::Preset::Girl},
  {"green", GotchiTheme::Preset::Green},
  {"gold",  GotchiTheme::Preset::Gold},
  {"red",   GotchiTheme::Preset::Red},
  {"white", GotchiTheme::Preset::White},
};
constexpr int PRESET_COUNT = sizeof(PRESETS) / sizeof(PRESETS[0]);

} // namespace

namespace GotchiTheme {

void setPreset(Preset p) {
  s_preset = p;
  applyPreset(p);
}

void setPresetByName(const char* name) {
  for (int i = 0; i < PRESET_COUNT; i++) {
    if (strcmp(name, PRESETS[i].name) == 0) {
      setPreset(PRESETS[i].preset);
      return;
    }
  }
  // Pas trouve → default boy
  setPreset(Preset::Boy);
}

void setCustomColor(uint16_t rgb565) {
  uint8_t r = ((rgb565 >> 11) & 0x1F);
  uint8_t g = ((rgb565 >> 5) & 0x3F);
  uint8_t b = (rgb565 & 0x1F);
  s_colors.eye     = rgb565;
  s_colors.inner   = ((r / 6) << 11) | ((g / 6) << 5) | (b / 6);
  s_colors.tongue  = 0xF890;
  s_colors.overlay = rgb565;
}

const Colors& getColors() {
  return s_colors;
}

Preset getCurrentPreset() {
  return s_preset;
}

const char* getPresetName() {
  for (int i = 0; i < PRESET_COUNT; i++) {
    if (PRESETS[i].preset == s_preset) return PRESETS[i].name;
  }
  return "boy";
}

void loadFromConfig() {
  SDConfig cfg = SDManager::getConfig();
  if (strlen(cfg.gotchi_theme) > 0) {
    setPresetByName(cfg.gotchi_theme);
    Serial.printf("[THEME] Charge depuis config: %s\n", cfg.gotchi_theme);
  } else {
    setPreset(Preset::Boy);
  }
}

void saveToConfig() {
  SDConfig cfg = SDManager::getConfig();
  strncpy(cfg.gotchi_theme, getPresetName(), sizeof(cfg.gotchi_theme) - 1);
  cfg.gotchi_theme[sizeof(cfg.gotchi_theme) - 1] = '\0';
  SDManager::saveConfig(cfg);
  Serial.printf("[THEME] Sauvegarde: %s\n", cfg.gotchi_theme);
}

} // namespace GotchiTheme
