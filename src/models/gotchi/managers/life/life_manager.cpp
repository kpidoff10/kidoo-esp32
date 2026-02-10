#include "life_manager.h"
#include "../../config/config.h"
#include "../../config/constants.h"
#include "../../../common/managers/sd/sd_manager.h"
#ifdef HAS_LCD
#include "../emotions/trigger_manager.h"
#endif
#include <ArduinoJson.h>
#include <SD.h>

// Static variables initialization
bool LifeManager::initialized = false;
GotchiStats LifeManager::stats = {
  STATS_HUNGER_INITIAL,
  STATS_HAPPINESS_INITIAL,
  STATS_HEALTH_INITIAL,
  STATS_FATIGUE_INITIAL,
  STATS_HYGIENE_INITIAL
};
ActionCooldowns LifeManager::cooldowns = {0, 0, 0, 0, 0, 0};
unsigned long LifeManager::lastUpdateTime = 0;
ActiveProgressiveEffect LifeManager::progressiveEffect = {"", 0, 0, 0, 0, 0, 0, false};

// ============================================
// Fonctions publiques
// ============================================

bool LifeManager::init() {
  Serial.println("[LifeManager] Initialisation...");

  // Charger l'état depuis la SD si disponible
  if (!loadState()) {
    Serial.println("[LifeManager] Aucun état sauvegardé trouvé, utilisation des valeurs par défaut");
    resetStats();
  }

  lastUpdateTime = millis();
  initialized = true;

  Serial.println("[LifeManager] Initialisation réussie");
  Serial.printf("[LifeManager] Stats initiales - Faim: %d, Bonheur: %d, Santé: %d, Fatigue: %d, Propreté: %d\n",
                stats.hunger, stats.happiness, stats.health, stats.fatigue, stats.hygiene);

  return true;
}

void LifeManager::update() {
  if (!initialized) {
    return;
  }

  // Vérifier si c'est le moment de décliner les stats (toutes les 30 minutes)
  unsigned long currentTime = millis();
  unsigned long elapsed = currentTime - lastUpdateTime;

  if (elapsed >= STATS_UPDATE_INTERVAL_MS) {
    Serial.println("[LifeManager] Déclin automatique des stats");
    declineStats();
    lastUpdateTime = currentTime;

    // Sauvegarder l'état après le déclin
    saveState();

    // Afficher les stats après déclin
    Serial.printf("[LifeManager] Stats après déclin - Faim: %d, Bonheur: %d, Santé: %d, Fatigue: %d, Propreté: %d\n",
                  stats.hunger, stats.happiness, stats.health, stats.fatigue, stats.hygiene);
  }

  // Mettre à jour l'effet progressif en cours
  updateProgressiveEffect();
}

GotchiStats LifeManager::getStats() {
  return stats;
}

bool LifeManager::applyAction(const String& actionId) {
  if (!initialized) {
    Serial.println("[LifeManager] Erreur: LifeManager non initialisé");
    return false;
  }

  // Vérifier si l'action est disponible (cooldown)
  if (!isActionAvailable(actionId)) {
    unsigned long remaining = getRemainingCooldown(actionId);
    Serial.printf("[LifeManager] Action '%s' en cooldown - %lu ms restants\n", actionId.c_str(), remaining);
    return false;
  }

  // Démarrer l'effet progressif
  bool applied = startProgressiveEffect(actionId);

  if (!applied) {
    Serial.printf("[LifeManager] Action inconnue: %s\n", actionId.c_str());
    return false;
  }

  // Enregistrer le timestamp du cooldown
  if (actionId == NFC_ITEM_BOTTLE) {
    cooldowns.lastBottle = millis();
  } else if (actionId == NFC_ITEM_SNACK) {
    cooldowns.lastSnack = millis();
  } else if (actionId == NFC_ITEM_WATER) {
    cooldowns.lastWater = millis();
  }

  // Sauvegarder l'état après le démarrage de l'action
  saveState();

  Serial.printf("[LifeManager] Effet progressif démarré pour '%s'\n", actionId.c_str());
  return true;
}

unsigned long LifeManager::getLastActionTime(const String& actionId) {
  if (actionId == NFC_ITEM_BOTTLE) {
    return cooldowns.lastBottle;
  } else if (actionId == NFC_ITEM_SNACK) {
    return cooldowns.lastSnack;
  } else if (actionId == NFC_ITEM_WATER) {
    return cooldowns.lastWater;
  }
  return 0;
}

bool LifeManager::isActionAvailable(const String& actionId) {
  unsigned long lastTime = getLastActionTime(actionId);
  if (lastTime == 0) {
    return true; // Jamais utilisé
  }

  unsigned long cooldownDuration = getCooldownDuration(actionId);
  unsigned long elapsed = millis() - lastTime;

  return elapsed >= cooldownDuration;
}

unsigned long LifeManager::getRemainingCooldown(const String& actionId) {
  if (isActionAvailable(actionId)) {
    return 0;
  }

  unsigned long lastTime = getLastActionTime(actionId);
  unsigned long cooldownDuration = getCooldownDuration(actionId);
  unsigned long elapsed = millis() - lastTime;

  return cooldownDuration - elapsed;
}

void LifeManager::forceStatDecline() {
  if (!initialized) {
    Serial.println("[LifeManager] Erreur: LifeManager non initialisé");
    return;
  }

  Serial.println("[LifeManager] Déclin forcé des stats");
  declineStats();
  saveState();

  Serial.printf("[LifeManager] Stats après déclin forcé - Faim: %d, Bonheur: %d, Santé: %d, Fatigue: %d, Propreté: %d\n",
                stats.hunger, stats.happiness, stats.health, stats.fatigue, stats.hygiene);
}

bool LifeManager::saveState() {
  if (!SDManager::isAvailable()) {
    Serial.println("[LifeManager] Erreur: SD non initialisée");
    return false;
  }

  // Créer le dossier /gotchi s'il n'existe pas
  if (!SD.exists("/gotchi")) {
    SD.mkdir("/gotchi");
  }

  // Créer un document JSON
  JsonDocument doc;

  // Ajouter les stats
  doc["faim"] = stats.hunger;
  doc["bonheur"] = stats.happiness;
  doc["sante"] = stats.health;
  doc["fatigue"] = stats.fatigue;
  doc["proprete"] = stats.hygiene;

  // Ajouter les cooldowns
  doc["lastBottle"] = (unsigned long long)cooldowns.lastBottle;
  doc["lastSnack"] = (unsigned long long)cooldowns.lastSnack;
  doc["lastWater"] = (unsigned long long)cooldowns.lastWater;

  // Ajouter le timestamp de dernière mise à jour
  doc["lastUpdateTime"] = (unsigned long long)lastUpdateTime;

  // Ajouter l'effet progressif en cours
  doc["progressiveEffect"]["active"] = progressiveEffect.active;
  if (progressiveEffect.active) {
    doc["progressiveEffect"]["itemId"] = progressiveEffect.itemId;
    doc["progressiveEffect"]["tickHunger"] = progressiveEffect.tickHunger;
    doc["progressiveEffect"]["tickHappiness"] = progressiveEffect.tickHappiness;
    doc["progressiveEffect"]["tickHealth"] = progressiveEffect.tickHealth;
    doc["progressiveEffect"]["tickInterval"] = (unsigned long long)progressiveEffect.tickInterval;
    doc["progressiveEffect"]["remainingTicks"] = progressiveEffect.remainingTicks;
    doc["progressiveEffect"]["lastTickTime"] = (unsigned long long)progressiveEffect.lastTickTime;
  }

  // Ouvrir le fichier en écriture
  File file = SD.open("/gotchi/life_state.json", FILE_WRITE);
  if (!file) {
    Serial.println("[LifeManager] Erreur: Impossible d'ouvrir le fichier pour écriture");
    return false;
  }

  // Sérialiser et écrire
  if (serializeJson(doc, file) == 0) {
    Serial.println("[LifeManager] Erreur: Échec de la sérialisation JSON");
    file.close();
    return false;
  }

  file.close();
  Serial.println("[LifeManager] État sauvegardé avec succès");
  return true;
}

bool LifeManager::loadState() {
  if (!SDManager::isAvailable()) {
    Serial.println("[LifeManager] Erreur: SD non initialisée");
    return false;
  }

  // Vérifier si le fichier existe
  if (!SD.exists("/gotchi/life_state.json")) {
    Serial.println("[LifeManager] Aucun état sauvegardé trouvé");
    return false;
  }

  // Ouvrir le fichier en lecture
  File file = SD.open("/gotchi/life_state.json", FILE_READ);
  if (!file) {
    Serial.println("[LifeManager] Erreur: Impossible d'ouvrir le fichier pour lecture");
    return false;
  }

  // Parser le JSON
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.printf("[LifeManager] Erreur de parsing JSON: %s\n", error.c_str());
    return false;
  }

  // Charger les stats
  stats.hunger = doc["faim"] | STATS_HUNGER_INITIAL;
  stats.happiness = doc["bonheur"] | STATS_HAPPINESS_INITIAL;
  stats.health = doc["sante"] | STATS_HEALTH_INITIAL;
  stats.fatigue = doc["fatigue"] | STATS_FATIGUE_INITIAL;
  stats.hygiene = doc["proprete"] | STATS_HYGIENE_INITIAL;

  // Charger les cooldowns
  cooldowns.lastBottle = doc["lastBottle"] | 0;
  cooldowns.lastSnack = doc["lastSnack"] | 0;
  cooldowns.lastWater = doc["lastWater"] | 0;

  // Charger le timestamp de dernière mise à jour
  lastUpdateTime = doc["lastUpdateTime"] | millis();

  // Charger l'effet progressif en cours
  progressiveEffect.active = doc["progressiveEffect"]["active"] | false;
  if (progressiveEffect.active) {
    const char* itemId = doc["progressiveEffect"]["itemId"] | "";
    strncpy(progressiveEffect.itemId, itemId, sizeof(progressiveEffect.itemId) - 1);
    progressiveEffect.itemId[sizeof(progressiveEffect.itemId) - 1] = '\0';
    progressiveEffect.tickHunger = doc["progressiveEffect"]["tickHunger"] | 0;
    progressiveEffect.tickHappiness = doc["progressiveEffect"]["tickHappiness"] | 0;
    progressiveEffect.tickHealth = doc["progressiveEffect"]["tickHealth"] | 0;
    progressiveEffect.tickInterval = doc["progressiveEffect"]["tickInterval"] | 0;
    progressiveEffect.remainingTicks = doc["progressiveEffect"]["remainingTicks"] | 0;
    progressiveEffect.lastTickTime = doc["progressiveEffect"]["lastTickTime"] | millis();

    Serial.printf("[LifeManager] Effet progressif en cours restauré: %s (%d ticks restants)\n",
                  progressiveEffect.itemId, progressiveEffect.remainingTicks);
  }

  Serial.println("[LifeManager] État chargé avec succès");
  return true;
}

void LifeManager::resetStats() {
  Serial.println("[LifeManager] Réinitialisation des stats");

  stats.hunger = STATS_HUNGER_INITIAL;
  stats.happiness = STATS_HAPPINESS_INITIAL;
  stats.health = STATS_HEALTH_INITIAL;
  stats.fatigue = STATS_FATIGUE_INITIAL;
  stats.hygiene = STATS_HYGIENE_INITIAL;

  cooldowns.lastBottle = 0;
  cooldowns.lastSnack = 0;
  cooldowns.lastWater = 0;
  cooldowns.lastToothbrush = 0;
  cooldowns.lastSoap = 0;
  cooldowns.lastBed = 0;

  // Réinitialiser l'effet progressif
  progressiveEffect.active = false;
  progressiveEffect.itemId[0] = '\0';
  progressiveEffect.remainingTicks = 0;

  lastUpdateTime = millis();

  // Sauvegarder l'état réinitialisé
  saveState();
}

bool LifeManager::adjustStat(const String& statName, int delta) {
  if (!initialized) {
    Serial.println("[LifeManager] Erreur: LifeManager non initialisé");
    return false;
  }

  String name = statName;
  name.toLowerCase();
  name.trim();

  uint8_t* statPtr = nullptr;

  if (name == "hunger") {
    statPtr = &stats.hunger;
  } else if (name == "happiness") {
    statPtr = &stats.happiness;
  } else if (name == "health") {
    statPtr = &stats.health;
  } else if (name == "fatigue") {
    statPtr = &stats.fatigue;
  } else if (name == "hygiene") {
    statPtr = &stats.hygiene;
  } else {
    Serial.printf("[LifeManager] Erreur: Stat '%s' inconnue\n", name.c_str());
    return false;
  }

  // Appliquer le delta
  int newValue = (int)*statPtr + delta;
  *statPtr = clampStat(newValue);

  Serial.printf("[LifeManager] Stat '%s' modifiée: %d -> %d (delta: %+d)\n",
                name.c_str(), newValue - delta, *statPtr, delta);

  // Sauvegarder l'état
  saveState();

  return true;
}

// ============================================
// Fonctions privées
// ============================================

void LifeManager::declineStats() {
  // Phase 1 : Uniquement le déclin de la faim
  int newFaim = (int)stats.hunger - STATS_HUNGER_DECLINE_RATE;
  stats.hunger = clampStat(newFaim);

  // TODO Phase 2+ : Ajouter le déclin des autres stats
  // int newBonheur = (int)stats.happiness - STATS_BONHEUR_DECLINE_RATE;
  // stats.happiness = clampStat(newBonheur);

  // int newFatigue = (int)stats.fatigue + STATS_FATIGUE_INCREASE_RATE;
  // stats.fatigue = clampStat(newFatigue);

  // int newProprete = (int)stats.hygiene - STATS_PROPRETE_DECLINE_RATE;
  // stats.hygiene = clampStat(newProprete);
}

bool LifeManager::applyBottle() {
  Serial.println("[LifeManager] Application du biberon");

  // Augmenter la faim
  int newFaim = (int)stats.hunger + NFC_BOTTLE_HUNGER_BONUS;
  stats.hunger = clampStat(newFaim);

  // Augmenter le bonheur
  int newBonheur = (int)stats.happiness + NFC_BOTTLE_HAPPINESS_BONUS;
  stats.happiness = clampStat(newBonheur);

  // Enregistrer le timestamp
  cooldowns.lastBottle = millis();

  Serial.printf("[LifeManager] Biberon appliqué - Faim: +%d, Bonheur: +%d\n",
                NFC_BOTTLE_HUNGER_BONUS, NFC_BOTTLE_HAPPINESS_BONUS);

  return true;
}

bool LifeManager::applySnack() {
  Serial.println("[LifeManager] Application du snack");

  // Augmenter la faim
  int newFaim = (int)stats.hunger + NFC_SNACK_HUNGER_BONUS;
  stats.hunger = clampStat(newFaim);

  // Augmenter le bonheur
  int newBonheur = (int)stats.happiness + NFC_SNACK_HAPPINESS_BONUS;
  stats.happiness = clampStat(newBonheur);

  // Enregistrer le timestamp
  cooldowns.lastSnack = millis();

  Serial.printf("[LifeManager] Snack appliqué - Faim: +%d, Bonheur: +%d\n",
                NFC_SNACK_HUNGER_BONUS, NFC_SNACK_HAPPINESS_BONUS);

  return true;
}

bool LifeManager::applyWater() {
  Serial.println("[LifeManager] Application de l'eau");

  // Augmenter la faim (hydratation)
  int newFaim = (int)stats.hunger + NFC_WATER_HUNGER_BONUS;
  stats.hunger = clampStat(newFaim);

  // Augmenter la santé
  int newSante = (int)stats.health + NFC_WATER_HEALTH_BONUS;
  stats.health = clampStat(newSante);

  // Enregistrer le timestamp
  cooldowns.lastWater = millis();

  Serial.printf("[LifeManager] Eau appliquée - Faim: +%d, Santé: +%d\n",
                NFC_WATER_HUNGER_BONUS, NFC_WATER_HEALTH_BONUS);

  return true;
}

uint8_t LifeManager::clampStat(int value) {
  if (value < STATS_MIN) {
    return STATS_MIN;
  } else if (value > STATS_MAX) {
    return STATS_MAX;
  }
  return (uint8_t)value;
}

unsigned long LifeManager::getCooldownDuration(const String& actionId) {
  if (actionId == NFC_ITEM_BOTTLE) {
    return NFC_BOTTLE_COOLDOWN_MS;
  } else if (actionId == NFC_ITEM_SNACK) {
    return NFC_SNACK_COOLDOWN_MS;
  } else if (actionId == NFC_ITEM_WATER) {
    return NFC_WATER_COOLDOWN_MS;
  }
  return 0;
}

bool LifeManager::startProgressiveEffect(const String& actionId) {
  // Rechercher l'effet progressif correspondant
  const ProgressiveFoodEffect* effect = nullptr;
  for (size_t i = 0; i < PROGRESSIVE_FOOD_EFFECTS_SIZE; i++) {
    if (actionId == PROGRESSIVE_FOOD_EFFECTS[i].itemId) {
      effect = &PROGRESSIVE_FOOD_EFFECTS[i];
      break;
    }
  }

  if (effect == nullptr) {
    return false;
  }

  // Configurer l'effet progressif
  strncpy(progressiveEffect.itemId, effect->itemId, sizeof(progressiveEffect.itemId) - 1);
  progressiveEffect.itemId[sizeof(progressiveEffect.itemId) - 1] = '\0';
  progressiveEffect.tickHunger = effect->tickHunger;
  progressiveEffect.tickHappiness = effect->tickHappiness;
  progressiveEffect.tickHealth = effect->tickHealth;
  progressiveEffect.tickInterval = effect->tickInterval;
  progressiveEffect.remainingTicks = effect->totalTicks;
  progressiveEffect.lastTickTime = millis();
  progressiveEffect.active = true;

  Serial.printf("[LifeManager] Effet progressif démarré: %s (%d ticks, intervalle %lu ms)\n",
                progressiveEffect.itemId, progressiveEffect.remainingTicks, progressiveEffect.tickInterval);

  // Déclencher le trigger "eating_started"
#ifdef HAS_LCD
  TriggerManager::checkTrigger("eating_started");
#endif

  // Appliquer le premier tick immédiatement
  applyProgressiveTick();

  return true;
}

void LifeManager::updateProgressiveEffect() {
  if (!progressiveEffect.active) {
    return;
  }

  unsigned long currentTime = millis();
  unsigned long elapsed = currentTime - progressiveEffect.lastTickTime;

  if (elapsed >= progressiveEffect.tickInterval) {
    applyProgressiveTick();
    progressiveEffect.lastTickTime = currentTime;
  }
}

void LifeManager::applyProgressiveTick() {
  if (!progressiveEffect.active || progressiveEffect.remainingTicks == 0) {
    return;
  }

  // Appliquer les bonus du tick
  int newHunger = (int)stats.hunger + progressiveEffect.tickHunger;
  stats.hunger = clampStat(newHunger);

  int newHappiness = (int)stats.happiness + progressiveEffect.tickHappiness;
  stats.happiness = clampStat(newHappiness);

  int newHealth = (int)stats.health + progressiveEffect.tickHealth;
  stats.health = clampStat(newHealth);

  progressiveEffect.remainingTicks--;

  Serial.printf("[LifeManager] Tick progressif appliqué: +%d hunger, +%d happiness, +%d health (ticks restants: %d)\n",
                progressiveEffect.tickHunger, progressiveEffect.tickHappiness, progressiveEffect.tickHealth,
                progressiveEffect.remainingTicks);

  // Si tous les ticks sont terminés, désactiver l'effet
  if (progressiveEffect.remainingTicks == 0) {
    Serial.printf("[LifeManager] Effet progressif '%s' terminé\n", progressiveEffect.itemId);

    // Déclencher le trigger "eating_finished" si le hunger est proche de 100%
    if (stats.hunger >= 90) {
#ifdef HAS_LCD
      TriggerManager::checkTrigger("eating_finished");
#endif
    }

    progressiveEffect.active = false;

    // Sauvegarder l'état final
    saveState();
  }
}
