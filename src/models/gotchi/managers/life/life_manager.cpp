#include "life_manager.h"
#include "../../config/config.h"
#include "../../config/constants.h"
#include "../../../common/managers/sd/sd_manager.h"
#if defined(HAS_NFC) && HAS_NFC
#include "../../../common/managers/nfc/nfc_manager.h"
#endif
#ifdef HAS_LCD
#include "../emotions/trigger_manager.h"
#endif
#include <ArduinoJson.h>
#include <SD.h>
#include <cstring>

// Static variables initialization
bool LifeManager::initialized = false;
GotchiStats LifeManager::stats = {
  STATS_HUNGER_INITIAL,
  STATS_HAPPINESS_INITIAL,
  STATS_HEALTH_INITIAL,
  STATS_FATIGUE_INITIAL,
  STATS_HYGIENE_INITIAL
};
ActionCooldowns LifeManager::cooldowns = {0, 0, 0, 0, 0, 0, 0};
unsigned long LifeManager::lastUpdateTime = 0;
ActiveProgressiveEffect LifeManager::progressiveEffect = {"", 0, 0, 0, 0, 0, 0, false};

// ============================================
// Fonctions publiques
// ============================================

bool LifeManager::init() {
  Serial.println("[LifeManager] Initialisation...");

  // Charger l'état depuis la SD (retry une fois si la SD a pu être occupée par EmotionManager juste avant)
  bool loaded = loadState();
  if (!loaded && SDManager::isAvailable()) {
    delay(400);
    loaded = loadState();
  }

  if (!loaded) {
    if (!SDManager::isAvailable()) {
      Serial.println("[LifeManager] SD non disponible - valeurs par défaut");
    } else if (!SD.exists("/gotchi/life_state.json")) {
      Serial.println("[LifeManager] Aucun fichier life_state.json - création avec valeurs par défaut");
      resetStats(true);
    } else {
      Serial.println("[LifeManager] Lecture du fichier échouée - valeurs par défaut en mémoire (fichier non écrasé)");
      resetStats(false);
    }
  } else {
    Serial.printf("[LifeManager] État restauré depuis /gotchi/life_state.json - Faim: %d, Bonheur: %d, Santé: %d, Fatigue: %d, Propreté: %d\n",
                  stats.hunger, stats.happiness, stats.health, stats.fatigue, stats.hygiene);
  }

  lastUpdateTime = millis();
  initialized = true;
  Serial.println("[LifeManager] Initialisation réussie");
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

  // 4 variants = 4 itemIds (snack = alias serial → cake)
  String effectId = (actionId == "snack") ? "cake" : actionId;

  // Démarrer l'effet progressif
  bool applied = startProgressiveEffect(effectId);

  if (!applied) {
    Serial.printf("[LifeManager] Action inconnue: %s\n", actionId.c_str());
    return false;
  }

  if (actionId == NFC_ITEM_BOTTLE) {
    cooldowns.lastBottle = millis();
  } else if (actionId == NFC_ITEM_CAKE || actionId == "snack") {
    cooldowns.lastCake = millis();
  } else if (actionId == NFC_ITEM_CANDY) {
    cooldowns.lastCandy = millis();
  } else if (actionId == NFC_ITEM_APPLE) {
    cooldowns.lastApple = millis();
  }

  // Sauvegarder l'état après le démarrage de l'action
  saveState();

  Serial.printf("[LifeManager] Effet progressif démarré pour '%s'\n", actionId.c_str());
  return true;
}

bool LifeManager::applyTriggerEffect(const String& triggerId) {
#if defined(TRIGGER_STAT_EFFECTS_SIZE) && TRIGGER_STAT_EFFECTS_SIZE > 0
  if (!initialized) return false;
  const char* idCstr = triggerId.c_str();
  for (size_t i = 0; i < TRIGGER_STAT_EFFECTS_SIZE; i++) {
    if (strcmp(TRIGGER_STAT_EFFECTS[i].triggerId, idCstr) != 0) continue;
    const struct TriggerStatEffect* e = &TRIGGER_STAT_EFFECTS[i].effect;
    if (e->hunger != 0)  stats.hunger   = clampStat((int)stats.hunger   + e->hunger);
    if (e->happiness != 0) stats.happiness = clampStat((int)stats.happiness + e->happiness);
    if (e->health != 0)   stats.health   = clampStat((int)stats.health   + e->health);
    if (e->fatigue != 0)  stats.fatigue  = clampStat((int)stats.fatigue  + e->fatigue);
    if (e->hygiene != 0)  stats.hygiene  = clampStat((int)stats.hygiene  + e->hygiene);
    saveState();
    Serial.printf("[LifeManager] Effet trigger '%s' appliqué (hunger=%d happiness=%d health=%d fatigue=%d hygiene=%d)\n",
                  idCstr, (int)e->hunger, (int)e->happiness, (int)e->health, (int)e->fatigue, (int)e->hygiene);
    return true;
  }
#endif
  return false;
}

unsigned long LifeManager::getLastActionTime(const String& actionId) {
  if (actionId == NFC_ITEM_BOTTLE) return cooldowns.lastBottle;
  if (actionId == NFC_ITEM_CAKE || actionId == "snack") return cooldowns.lastCake;
  if (actionId == NFC_ITEM_CANDY) return cooldowns.lastCandy;
  if (actionId == NFC_ITEM_APPLE) return cooldowns.lastApple;
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
    if (!SD.mkdir("/gotchi")) {
      Serial.println("[LifeManager] Erreur: Impossible de creer le dossier /gotchi sur la SD");
      return false;
    }
  }
  if (!SD.exists("/gotchi")) {
    Serial.println("[LifeManager] Erreur: Dossier /gotchi absent apres mkdir (carte en lecture seule?)");
    return false;
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
  doc["lastCake"] = (unsigned long long)cooldowns.lastCake;
  doc["lastCandy"] = (unsigned long long)cooldowns.lastCandy;
  doc["lastApple"] = (unsigned long long)cooldowns.lastApple;

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
    Serial.println("[LifeManager] Erreur: Impossible d'ouvrir /gotchi/life_state.json en ecriture (carte pleine ou en lecture seule?)");
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
  cooldowns.lastCake = doc["lastCake"] | 0;
  cooldowns.lastCandy = doc["lastCandy"] | 0;
  cooldowns.lastApple = doc["lastApple"] | doc["lastWater"] | 0;  // lastWater = rétrocompat

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

  return true;
}

void LifeManager::resetStats(bool saveToFile) {
  Serial.println("[LifeManager] Réinitialisation des stats");

  stats.hunger = STATS_HUNGER_INITIAL;
  stats.happiness = STATS_HAPPINESS_INITIAL;
  stats.health = STATS_HEALTH_INITIAL;
  stats.fatigue = STATS_FATIGUE_INITIAL;
  stats.hygiene = STATS_HYGIENE_INITIAL;

  cooldowns.lastBottle = 0;
  cooldowns.lastCake = 0;
  cooldowns.lastCandy = 0;
  cooldowns.lastApple = 0;
  cooldowns.lastToothbrush = 0;
  cooldowns.lastSoap = 0;
  cooldowns.lastBed = 0;

  // Réinitialiser l'effet progressif
  progressiveEffect.active = false;
  progressiveEffect.itemId[0] = '\0';
  progressiveEffect.remainingTicks = 0;

  lastUpdateTime = millis();

  if (saveToFile) {
    saveState();
  }
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
  // Faim : déclin de base
  int newFaim = (int)stats.hunger - STATS_HUNGER_DECLINE_RATE;
  stats.hunger = clampStat(newFaim);

  // Humeur (bonheur) : déclin de base + bonus si reste longtemps sans manger (faim basse)
  int happinessDecay = STATS_HAPPINESS_DECLINE_RATE;
  if (stats.hunger < STATS_HUNGER_THRESHOLD_CRITICAL) {
    happinessDecay += STATS_HAPPINESS_DECLINE_BONUS_CRITICAL;
  } else if (stats.hunger < STATS_HUNGER_THRESHOLD_LOW) {
    happinessDecay += STATS_HAPPINESS_DECLINE_BONUS_LOW;
  }
  int newBonheur = (int)stats.happiness - happinessDecay;
  stats.happiness = clampStat(newBonheur);

  // Propreté (hygiène) : déclin de base + bonus si reste longtemps sans manger
  int hygieneDecay = STATS_HYGIENE_DECLINE_RATE;
  if (stats.hunger < STATS_HUNGER_THRESHOLD_CRITICAL) {
    hygieneDecay += STATS_HYGIENE_DECLINE_BONUS_CRITICAL;
  } else if (stats.hunger < STATS_HUNGER_THRESHOLD_LOW) {
    hygieneDecay += STATS_HYGIENE_DECLINE_BONUS_LOW;
  }
  int newProprete = (int)stats.hygiene - hygieneDecay;
  stats.hygiene = clampStat(newProprete);

  // Santé (vie) : -1 toutes les 30 min si très faim (faim < seuil critique)
  if (stats.hunger < STATS_HUNGER_THRESHOLD_CRITICAL) {
    int newSante = (int)stats.health - STATS_HEALTH_DECLINE_WHEN_VERY_HUNGRY;
    stats.health = clampStat(newSante);
  }
}

bool LifeManager::applyBottle() {
  return applyAction(NFC_ITEM_BOTTLE);
}

bool LifeManager::applySnack() {
  return applyAction(NFC_ITEM_CAKE);  // snack serial = gâteau
}

bool LifeManager::applyApple() {
  return applyAction(NFC_ITEM_APPLE);
}

bool LifeManager::applyFirstAvailableFood() {
  if (applyBottle()) return true;
  if (applySnack()) return true;   // cake
  if (applyApple()) return true;
  if (applyAction(NFC_ITEM_CANDY)) return true;
  return false;
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
  if (actionId == NFC_ITEM_BOTTLE) return NFC_BOTTLE_COOLDOWN_MS;
  if (actionId == NFC_ITEM_CAKE || actionId == "snack") return NFC_CAKE_COOLDOWN_MS;
  if (actionId == NFC_ITEM_CANDY) return NFC_CANDY_COOLDOWN_MS;
  if (actionId == NFC_ITEM_APPLE) return NFC_APPLE_COOLDOWN_MS;
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
  // totalTicks == 0 (biberon) = illimité : on utilise 255 comme sentinelle, arrêt à faim 100% ou stopProgressiveEffect()
  progressiveEffect.remainingTicks = (effect->totalTicks == 0) ? 255 : effect->totalTicks;
  progressiveEffect.lastTickTime = millis();
  progressiveEffect.active = true;

  if (effect->totalTicks == 0) {
    Serial.printf("[LifeManager] Effet progressif démarré: %s (illimité, intervalle %lu ms)\n",
                  progressiveEffect.itemId, progressiveEffect.tickInterval);
  } else {
    Serial.printf("[LifeManager] Effet progressif démarré: %s (%d ticks, intervalle %lu ms)\n",
                  progressiveEffect.itemId, progressiveEffect.remainingTicks, progressiveEffect.tickInterval);
  }

  // Déclencher le trigger "eating" avec le variant correspondant à l'aliment (bottle=1, cake=2, apple=3, candy=4)
#ifdef HAS_LCD
  int variant = 1;
  if (actionId == "bottle") variant = 1;
  else if (actionId == "cake" || actionId == "snack") variant = 2;
  else if (actionId == "apple") variant = 3;
  else if (actionId == "candy") variant = 4;
  TriggerManager::setRequestedVariant(variant);
  TriggerManager::checkTrigger("eating");
#endif

  // Appliquer le premier tick immédiatement
  applyProgressiveTick();

  return true;
}

void LifeManager::updateProgressiveEffect() {
  if (!progressiveEffect.active) {
    return;
  }

#if defined(HAS_NFC) && HAS_NFC
  // Biberon : arrêter tout de suite si le tag NFC a été retiré (ne pas attendre GotchiNFCHandler::update)
  if (strcmp(progressiveEffect.itemId, "bottle") == 0 && NFCManager::isAvailable() && !NFCManager::isTagPresent()) {
    stopProgressiveEffect("bottle");
    return;
  }
#endif

  unsigned long currentTime = millis();
  unsigned long elapsed = currentTime - progressiveEffect.lastTickTime;

  if (elapsed >= progressiveEffect.tickInterval) {
    applyProgressiveTick();
    progressiveEffect.lastTickTime = currentTime;
  }
}

void LifeManager::stopProgressiveEffect(const String& actionId) {
  if (!progressiveEffect.active) {
    return;
  }
  if (strcmp(progressiveEffect.itemId, actionId.c_str()) != 0) {
    return;
  }
  Serial.printf("[LifeManager] Effet progressif '%s' arrêté (ex. tag retiré)\n", progressiveEffect.itemId);
  progressiveEffect.active = false;
  progressiveEffect.remainingTicks = 0;
  saveState();
}

bool LifeManager::isProgressiveEffectActive(const String& actionId) {
  if (!initialized || !progressiveEffect.active) {
    return false;
  }
  return (actionId == progressiveEffect.itemId);
}

void LifeManager::applyProgressiveTick() {
  if (!progressiveEffect.active) {
    return;
  }
  // remainingTicks == 0 avec effet actif = fin normale (ou déjà arrêté)
  if (progressiveEffect.remainingTicks == 0) {
    return;
  }

  // Appliquer les bonus du tick
  int newHunger = (int)stats.hunger + progressiveEffect.tickHunger;
  stats.hunger = clampStat(newHunger);

  int newHappiness = (int)stats.happiness + progressiveEffect.tickHappiness;
  stats.happiness = clampStat(newHappiness);

  int newHealth = (int)stats.health + progressiveEffect.tickHealth;
  stats.health = clampStat(newHealth);

  // Biberon (illimité) : remainingTicks == 255, on ne décrémente pas ; arrêt à faim 100%
  bool unlimitedBottle = (strcmp(progressiveEffect.itemId, "bottle") == 0 && progressiveEffect.remainingTicks == 255);
  if (!unlimitedBottle) {
    progressiveEffect.remainingTicks--;
  }

  if (unlimitedBottle) {
    Serial.printf("[LifeManager] Tick progressif appliqué: +%d hunger, +%d happiness, +%d health (biberon illimité)\n",
                  progressiveEffect.tickHunger, progressiveEffect.tickHappiness, progressiveEffect.tickHealth);
  } else {
    Serial.printf("[LifeManager] Tick progressif appliqué: +%d hunger, +%d happiness, +%d health (ticks restants: %d)\n",
                  progressiveEffect.tickHunger, progressiveEffect.tickHappiness, progressiveEffect.tickHealth,
                  progressiveEffect.remainingTicks);
  }

  // Fin : ticks épuisés OU biberon et faim à 100%
  bool finished = (progressiveEffect.remainingTicks == 0) ||
                  (unlimitedBottle && stats.hunger >= 100);

  if (finished) {
    if (unlimitedBottle && stats.hunger >= 100) {
      progressiveEffect.remainingTicks = 0;
    }
    Serial.printf("[LifeManager] Effet progressif '%s' terminé\n", progressiveEffect.itemId);

    // Fin de manger : pas de second trigger, une seule animation "Mange" au début

    progressiveEffect.active = false;
    saveState();
  }
}
