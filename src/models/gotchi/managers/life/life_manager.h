#ifndef LIFE_MANAGER_H
#define LIFE_MANAGER_H

#include <Arduino.h>

/**
 * Gestionnaire du système de vie Tamagotchi pour le modèle Gotchi
 *
 * Ce manager gère les stats de vie du Gotchi :
 * - Faim (0-100) : diminue automatiquement, se remplit avec nourriture
 * - Bonheur (0-100) : diminue automatiquement, augmente avec interactions
 * - Santé (0-100) : affectée par les autres stats
 * - Fatigue (0-100) : augmente automatiquement, diminue avec sommeil
 * - Propreté (0-100) : diminue automatiquement, augmente avec hygiène
 *
 * Phase 1 : Uniquement système de faim implémenté
 *
 * Fonctionnalités:
 * - Déclin automatique des stats toutes les 30 minutes
 * - Actions pour nourrir (bottle/cake/apple/candy)
 * - Cooldowns pour chaque action
 * - Sauvegarde/chargement de l'état sur SD
 * - Commandes Serial pour simulation (phase 1)
 */

// Structure for Gotchi stats
struct GotchiStats {
  uint8_t hunger;     // 0-100 (100 = full, 0 = starving)
  uint8_t happiness;  // 0-100 (100 = very happy, 0 = sad)
  uint8_t health;     // 0-100 (100 = healthy, 0 = sick)
  uint8_t fatigue;    // 0-100 (0 = rested, 100 = exhausted)
  uint8_t hygiene;    // 0-100 (100 = clean, 0 = dirty)
};

// Structure pour les cooldowns des actions
struct ActionCooldowns {
  unsigned long lastBottle;
  unsigned long lastCake;
  unsigned long lastCandy;
  unsigned long lastApple;
  unsigned long lastToothbrush;
  unsigned long lastSoap;
  unsigned long lastBed;
};

// Structure pour l'effet progressif en cours
struct ActiveProgressiveEffect {
  char itemId[16];              // ID de l'item actif ("bottle", "cake", "apple", "candy")
  uint8_t tickHunger;           // Hunger donné par tick
  uint8_t tickHappiness;        // Happiness donné par tick
  uint8_t tickHealth;           // Health donné par tick
  unsigned long tickInterval;   // Intervalle entre chaque tick (ms)
  uint8_t remainingTicks;       // Nombre de ticks restants
  unsigned long lastTickTime;   // Timestamp du dernier tick
  bool active;                  // true si un effet est en cours
};

class LifeManager {
public:
  /**
   * Initialiser le gestionnaire de vie
   * @return true si l'initialisation est réussie, false sinon
   */
  static bool init();

  /**
   * Mettre à jour le gestionnaire (à appeler périodiquement dans loop())
   * Déclenche le déclin automatique des stats selon l'intervalle configuré
   */
  static void update();

  /**
   * Obtenir les stats actuelles du Gotchi
   * @return Structure GotchiStats avec toutes les stats
   */
  static GotchiStats getStats();

  /**
   * Appliquer une action (nourrir, etc.)
   * @param actionId ID de l'action ("bottle", "cake", "apple", "candy")
   * @return true si l'action a été appliquée, false si cooldown actif ou action invalide
   */
  static bool applyAction(const String& actionId);

  /**
   * Appliquer l'effet instantané d'un trigger (ex. head_caress -> +1 bonheur).
   * Utilise TRIGGER_STAT_EFFECTS généré depuis kidoo-shared.
   * @param triggerId ID du trigger ("head_caress", etc.)
   * @return true si un effet a été appliqué, false sinon
   */
  static bool applyTriggerEffect(const String& triggerId);

  /**
   * Obtenir le timestamp de la dernière utilisation d'une action
   * @param actionId ID de l'action
   * @return Timestamp en millisecondes (0 si jamais utilisé)
   */
  static unsigned long getLastActionTime(const String& actionId);

  /**
   * Vérifier si une action est disponible (cooldown écoulé)
   * @param actionId ID de l'action
   * @return true si l'action est disponible, false si en cooldown
   */
  static bool isActionAvailable(const String& actionId);

  /**
   * Obtenir le temps restant avant disponibilité d'une action (en millisecondes)
   * @param actionId ID de l'action
   * @return Temps restant en ms, 0 si disponible
   */
  static unsigned long getRemainingCooldown(const String& actionId);

  /**
   * Forcer le déclin des stats immédiatement (pour tests)
   * Utile pour tester le système sans attendre 30 minutes
   */
  static void forceStatDecline();

  /**
   * Sauvegarder l'état actuel sur la carte SD
   * @return true si la sauvegarde a réussi, false sinon
   */
  static bool saveState();

  /**
   * Charger l'état depuis la carte SD
   * @return true si le chargement a réussi, false sinon
   */
  static bool loadState();

  /**
   * Arrêter un effet progressif en cours (ex. biberon quand le tag NFC est retiré).
   * @param actionId ID de l'action ("bottle", "cake", "apple", "candy")
   */
  static void stopProgressiveEffect(const String& actionId);

  /**
   * Réinitialiser toutes les stats aux valeurs par défaut
   * @param saveToFile si true (défaut), écrit l'état sur la SD ; si false, ne modifie pas le fichier (évite d'écraser un save corrompu)
   */
  static void resetStats(bool saveToFile = true);

  /**
   * Modifier une stat manuellement (pour tests)
   * @param statName Nom de la stat ("faim", "bonheur", "sante", "fatigue", "proprete")
   * @param delta Valeur à ajouter (peut être négative)
   * @return true si la stat a été modifiée, false si nom invalide
   */
  static bool adjustStat(const String& statName, int delta);

  /**
   * Appliquer le premier aliment disponible (pour gotchi-feed sans type / "any").
   * Ordre : bottle, cake, apple, candy.
   * @return true si un aliment a été appliqué, false si tous en cooldown
   */
  static bool applyFirstAvailableFood();

private:
  // Variables statiques
  static bool initialized;
  static GotchiStats stats;
  static ActionCooldowns cooldowns;
  static unsigned long lastUpdateTime;
  static ActiveProgressiveEffect progressiveEffect;

  // Fonctions privées

  /**
   * Décliner les stats selon les taux configurés
   * Appelé automatiquement toutes les 30 minutes
   */
  static void declineStats();

  /**
   * Appliquer l'effet d'un biberon
   * @return true si appliqué, false si en cooldown
   */
  static bool applyBottle();

  /**
   * Appliquer l'effet d'un snack
   * @return true si appliqué, false si en cooldown
   */
  static bool applySnack();

  /**
   * Appliquer l'effet pomme (fruit)
   * @return true si appliqué, false si en cooldown
   */
  static bool applyApple();

  /**
   * Limiter une stat entre STATS_MIN et STATS_MAX
   * @param value Valeur à limiter
   * @return Valeur limitée
   */
  static uint8_t clampStat(int value);

  /**
   * Obtenir le cooldown en millisecondes pour une action
   * @param actionId ID de l'action
   * @return Cooldown en ms, 0 si action inconnue
   */
  static unsigned long getCooldownDuration(const String& actionId);

  /**
   * Démarrer un effet progressif
   * @param actionId ID de l'action
   * @return true si l'effet a démarré, false sinon
   */
  static bool startProgressiveEffect(const String& actionId);

  /**
   * Mettre à jour l'effet progressif en cours
   * Appelé dans update() pour appliquer les ticks
   */
  static void updateProgressiveEffect();

  /**
   * Appliquer un tick de l'effet progressif
   */
  static void applyProgressiveTick();
};

#endif // LIFE_MANAGER_H
