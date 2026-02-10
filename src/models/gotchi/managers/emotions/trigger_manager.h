#ifndef TRIGGER_MANAGER_H
#define TRIGGER_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <map>

/**
 * Gestionnaire de triggers automatiques pour les émotions
 *
 * Ce manager surveille les stats du Gotchi et déclenche automatiquement
 * des animations quand certaines conditions sont remplies (ex: hunger ≤ 20%).
 *
 * Fonctionnalités:
 * - Charger la config des émotions avec leurs triggers
 * - Surveiller les stats périodiquement
 * - Détecter les changements de triggers
 * - Sélectionner aléatoirement parmi les émotions avec le même trigger
 * - Cooldown global pour éviter de spam les animations
 */

/** Structure d'une émotion indexée par trigger */
struct IndexedEmotion {
  String key;         // Clé de l'émotion (OK, SLEEP, etc.)
  String emotionId;   // UUID de l'émotion
  String trigger;     // Trigger (hunger_low, eating_finished, etc.)
  int variant;        // Variant (1-4) pour plusieurs animations par trigger
};

class TriggerManager {
public:
  /**
   * Initialiser le gestionnaire de triggers
   * Charge la config des émotions et construit l'index des triggers
   * @return true si l'initialisation a réussi, false sinon
   */
  static bool init();

  /**
   * Mettre à jour le gestionnaire (à appeler périodiquement dans loop())
   * Surveille les stats et déclenche les animations si nécessaire
   */
  static void update();

  /**
   * Vérifier manuellement si un trigger doit être activé
   * Utile pour les événements (ex: début de nourriture)
   * @param triggerName Nom du trigger (ex: "eating_started")
   */
  static void checkTrigger(const String& triggerName);

  /**
   * Obtenir le nombre d'émotions pour un trigger donné
   * @param triggerName Nom du trigger
   * @return Nombre d'émotions avec ce trigger
   */
  static int getEmotionCountForTrigger(const String& triggerName);

  /**
   * Activer/désactiver le système de triggers automatiques
   * @param enabled true pour activer, false pour désactiver
   */
  static void setEnabled(bool enabled);

  /**
   * Vérifier si le système de triggers est activé
   * @return true si activé, false sinon
   */
  static bool isEnabled();

  /**
   * Obtenir le variant actuellement demandé par le dernier trigger
   * @return Variant (1-4), ou 0 si aucun trigger actif
   */
  static int getRequestedVariant();

private:
  static bool _initialized;
  static bool _enabled;
  static unsigned long _lastCheckTime;
  static unsigned long _lastTriggerTime;
  static String _lastActiveTrigger;
  static int _requestedVariant;  // Variant demandé par le dernier trigger (1-4)

  // Map: trigger name → list of emotions
  static std::map<String, std::vector<IndexedEmotion>> _triggerIndex;

  /**
   * Charger la config des émotions et construire l'index des triggers
   * @return true si le chargement a réussi, false sinon
   */
  static bool loadTriggerConfig();

  /**
   * Évaluer si un trigger doit être activé selon les stats actuelles
   * @param triggerName Nom du trigger à évaluer (ex: "hunger_low")
   * @return true si le trigger doit être activé, false sinon
   */
  static bool evaluateTrigger(const String& triggerName);

  /**
   * Activer un trigger : sélectionner une émotion aléatoire et la jouer
   * @param triggerName Nom du trigger
   */
  static void activateTrigger(const String& triggerName);

  /**
   * Sélectionner aléatoirement une émotion parmi celles avec un trigger donné
   * @param triggerName Nom du trigger
   * @return Clé de l'émotion sélectionnée, ou String vide si aucune
   */
  static String selectRandomEmotion(const String& triggerName);

  /**
   * Vérifier si le cooldown global est écoulé
   * @return true si on peut déclencher une nouvelle animation, false sinon
   */
  static bool isCooldownElapsed();
};

#endif // TRIGGER_MANAGER_H
