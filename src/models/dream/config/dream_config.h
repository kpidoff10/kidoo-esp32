#ifndef MODEL_DREAM_DREAM_CONFIG_H
#define MODEL_DREAM_DREAM_CONFIG_H

#include <Arduino.h>
#include <cstdint>
#include "common/config/config_sizes.h"

/**
 * Configuration spécifique au modèle Dream
 * Stockée dans config.json sous la clé "dream"
 *
 * Sépare la config Dream de la config commune (SDConfig)
 * pour une gestion par modèle propre.
 */

struct DreamConfig {
  /** Alerte réveil nocturne : notification si l'enfant touche la veilleuse pendant la nuit */
  bool nighttime_alert_enabled;

  /** Configuration de couleur/effet par défaut (au tap, si aucune routine) */
  uint8_t default_color_r;      // 0-255 (défaut: 255)
  uint8_t default_color_g;      // 0-255 (défaut: 0)
  uint8_t default_color_b;      // 0-255 (défaut: 0)
  uint8_t default_brightness;   // 0-100 (défaut: 50)
  char default_effect[32];      // "" = couleur unie, "pulse_fast", "rainbow_soft", etc. (défaut: "")
};

class DreamConfigManager {
public:
  /**
   * Lire la configuration Dream depuis config.json (clé "dream")
   * Retourne les valeurs par défaut si la clé n'existe pas
   */
  static DreamConfig getConfig();

  /**
   * Sauvegarder la configuration Dream dans config.json
   * Merge avec le fichier existant (préserve les autres clés)
   */
  static bool saveConfig(const DreamConfig& config);

  /**
   * Initialiser une DreamConfig avec les valeurs par défaut
   */
  static void initDefaultConfig(DreamConfig* config);
};

#endif // MODEL_DREAM_DREAM_CONFIG_H
