#ifndef MODEL_DREAM_DREAM_CONFIG_H
#define MODEL_DREAM_DREAM_CONFIG_H

#include <Arduino.h>

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
