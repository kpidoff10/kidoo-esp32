#ifndef MODEL_GOTCHI_GOTCHI_CONFIG_H
#define MODEL_GOTCHI_GOTCHI_CONFIG_H

#include <cstdint>

/**
 * Configuration spécifique au modèle Gotchi (persistance des stats vivantes)
 * Stockée dans config.json sous la clé "gotchi"
 *
 * Sépare les stats Gotchi de la config commune (SDConfig). Permet au gotchi
 * de conserver sa faim/énergie/etc. à travers les redémarrages, et d'appliquer
 * un decay rétroactif basé sur le temps écoulé (RTC) pendant qu'il était éteint.
 */

struct GotchiStatsConfig {
  bool valid;             // True si chargé depuis disque

  // Besoins vitaux (0..100)
  float hunger;
  float energy;
  float happiness;
  float health;
  float hygiene;

  // États temporaires
  float boredom;
  float irritability;

  // Méta
  uint32_t ageMinutes;    // age cumulé du gotchi
  uint32_t lastSavedAt;   // epoch UTC en secondes (pour decay offline)
};

class GotchiConfigManager {
public:
  /**
   * Lire les stats depuis config.json (clé "gotchi"). Retourne valid=false
   * si la clé n'existe pas → l'appelant doit utiliser les défauts BehaviorStats.
   */
  static GotchiStatsConfig getStats();

  /**
   * Sauvegarder les stats dans config.json. Met automatiquement à jour
   * lastSavedAt avec le temps RTC courant.
   * Merge avec le fichier existant (préserve les autres clés).
   */
  static bool saveStats(const GotchiStatsConfig& stats);

  /**
   * Initialise une struct avec les valeurs par défaut (gotchi neuf).
   */
  static void initDefault(GotchiStatsConfig* stats);
};

#endif // MODEL_GOTCHI_GOTCHI_CONFIG_H
