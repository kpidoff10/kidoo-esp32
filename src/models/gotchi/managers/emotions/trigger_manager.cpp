#include "trigger_manager.h"
#include "emotion_manager.h"
#include "../life/life_manager.h"
#include "../../../model_config.h"
#include <set>

// Mettre à 1 pour debug (attention: flood Serial, accès console difficile)
#define TRIGGER_DEBUG_SERIAL 0

#if defined(HAS_SD)
#include <SD.h>
#include <ArduinoJson.h>
#endif

// Constantes
#define TRIGGER_CHECK_INTERVAL 5000     // Vérifier les triggers toutes les 5 secondes
#define TRIGGER_COOLDOWN 30000          // 30 secondes entre deux triggers automatiques
#define IDLE_OK_DELAY_MIN_MS 5000       // Délai min avant prochaine émotion OK (attente)
#define IDLE_OK_DELAY_MAX_MS 30000      // Délai max avant prochaine émotion OK (attente)
#define MIN_OK_BETWEEN_DEMAND_TRIGGERS 4  // Au moins 4 animations OK entre deux "demandes" (faim, gâteau, etc.)

// Variables statiques
bool TriggerManager::_initialized = false;
bool TriggerManager::_enabled = true;
unsigned long TriggerManager::_lastCheckTime = 0;
unsigned long TriggerManager::_lastTriggerTime = 0;
unsigned long TriggerManager::_lastIdleOkTime = 0;
unsigned long TriggerManager::_nextIdleOkDelayMs = IDLE_OK_DELAY_MIN_MS;  // Premier délai (sera random après 1re OK)
String TriggerManager::_lastActiveTrigger = "";
int TriggerManager::_requestedVariant = 0;
int TriggerManager::_idleOkCountSinceLastDemand = MIN_OK_BETWEEN_DEMAND_TRIGGERS;  // Premier démarrage : une demande peut jouer tout de suite
std::map<String, std::vector<IndexedEmotion>> TriggerManager::_triggerIndex;
// Triggers pour lesquels on a déjà logué "Aucune emotion" (éviter le flood Serial)
static std::set<String> s_loggedMissingTriggers;

bool TriggerManager::init() {
  _initialized = false;
  _enabled = true;
  _lastCheckTime = 0;
  _lastTriggerTime = 0;
  _lastIdleOkTime = 0;
  _nextIdleOkDelayMs = (unsigned long)random(IDLE_OK_DELAY_MIN_MS, IDLE_OK_DELAY_MAX_MS + 1);
  _lastActiveTrigger = "";
  _idleOkCountSinceLastDemand = MIN_OK_BETWEEN_DEMAND_TRIGGERS;
  _triggerIndex.clear();

#if defined(HAS_SD)
  Serial.println("[TRIGGER] Initialisation du gestionnaire de triggers...");

  if (!loadTriggerConfig()) {
    Serial.println("[TRIGGER] Erreur: Impossible de charger la config des triggers");
    return false;
  }

  _initialized = true;
  Serial.printf("[TRIGGER] %d triggers indexes, systeme pret\n", _triggerIndex.size());
  return true;
#else
  Serial.println("[TRIGGER] SD non disponible");
  return false;
#endif
}

bool TriggerManager::loadTriggerConfig() {
#if defined(HAS_SD)
  // Lire le characterId depuis /config.json
  File configFile = SD.open("/config.json", FILE_READ);
  if (!configFile) {
    Serial.println("[TRIGGER] Erreur: /config.json introuvable");
    return false;
  }

  JsonDocument configDoc;
  DeserializationError error = deserializeJson(configDoc, configFile);
  configFile.close();

  if (error) {
    Serial.printf("[TRIGGER] Erreur parsing config.json: %s\n", error.c_str());
    return false;
  }

  String characterId = configDoc["characterId"].as<String>();
  if (characterId.isEmpty()) {
    Serial.println("[TRIGGER] Erreur: characterId manquant dans config.json");
    return false;
  }

  // Charger la config des émotions
  String emotionsConfigPath = "/characters/" + characterId + "/emotions/config.json";
  File emotionsFile = SD.open(emotionsConfigPath.c_str(), FILE_READ);
  if (!emotionsFile) {
    Serial.printf("[TRIGGER] Erreur: %s introuvable\n", emotionsConfigPath.c_str());
    return false;
  }

  // Parser le JSON (tableau d'émotions)
  JsonDocument emotionsDoc;
  error = deserializeJson(emotionsDoc, emotionsFile);
  emotionsFile.close();

  if (error) {
    Serial.printf("[TRIGGER] Erreur parsing emotions config: %s\n", error.c_str());
    return false;
  }

  // Construire l'index des triggers
  JsonArray emotions = emotionsDoc.as<JsonArray>();
  int emotionCount = 0;

  for (JsonObject emotionObj : emotions) {
    String key = emotionObj["key"].as<String>();
    String emotionId = emotionObj["emotionId"].as<String>();
    String trigger = emotionObj["trigger"].as<String>();
    int variant = emotionObj["variant"] | 1;  // Par défaut: 1

    // Si trigger est vide ou null, utiliser "manual"
    if (trigger.isEmpty()) {
      trigger = "manual";
    }

    // Créer l'émotion indexée
    IndexedEmotion emotion;
    emotion.key = key;
    emotion.emotionId = emotionId;
    emotion.trigger = trigger;
    emotion.variant = variant;

    // Ajouter à l'index (map de trigger → liste d'émotions)
    _triggerIndex[trigger].push_back(emotion);
    emotionCount++;
#if TRIGGER_DEBUG_SERIAL
    Serial.printf("[TRIGGER] Indexe: %s -> %s (trigger: %s, variant: %d)\n",
                  key.c_str(), emotionId.c_str(), trigger.c_str(), variant);
#endif
  }

#if TRIGGER_DEBUG_SERIAL
  Serial.printf("[TRIGGER] %d emotions indexees\n", emotionCount);
#endif
  return true;
#else
  return false;
#endif
}

void TriggerManager::update() {
  if (!_initialized || !_enabled) {
    return;
  }

  unsigned long now = millis();

  // Vérifier les triggers selon l'intervalle défini
  if (now - _lastCheckTime < TRIGGER_CHECK_INTERVAL) {
    return;
  }

  _lastCheckTime = now;

  // Ne pas évaluer les triggers si une animation est déjà en cours
  if (EmotionManager::isPlaying()) {
    return;
  }

  // Parcourir les triggers par priorité. On ne sort que si on a vraiment joué une émotion
  // (pas si on a skip à cause du cooldown), pour pouvoir jouer des OK entre deux triggers humeur.
  // Liste alignée avec kidoo-shared/emotions/triggers.ts (site admin)
  const String priorityTriggers[] = {
    "hunger_critical",
    "health_critical",
    "hunger_low",
    "health_low",
    "happiness_low",
    "fatigue_high",
    "hygiene_low",
    "eating",
    "happiness_high",
    "health_good",
    "fatigue_low",
    "hygiene_good",
    "hunger_medium",
    "happiness_medium"
  };

  for (const String& triggerName : priorityTriggers) {
    if (evaluateTrigger(triggerName)) {
      if (activateTrigger(triggerName)) {
        if (isDemandTrigger(triggerName)) {
          _idleOkCountSinceLastDemand = 0;  // On vient de jouer une "demande", il faudra 4 OK avant la prochaine
        }
        return;  // Une émotion a été demandée, on sort
      }
      // Cooldown ou pas assez d'OK : on continue la boucle, puis on pourra jouer une OK
    }
  }

  // Aucun trigger humeur joué ce cycle : jouer une émotion OK (attente). OK = key "OK" + trigger "manual", pas de variant.
  if ((now - _lastIdleOkTime) >= _nextIdleOkDelayMs) {
    if (EmotionManager::requestEmotion("OK", 1, EMOTION_PRIORITY_NORMAL, 0)) {  // variant=0 = n'importe quelle OK en config
      _lastIdleOkTime = now;
      _lastTriggerTime = now;
      _idleOkCountSinceLastDemand++;  // Une OK de plus avant la prochaine "demande"
      _nextIdleOkDelayMs = (unsigned long)random(IDLE_OK_DELAY_MIN_MS, IDLE_OK_DELAY_MAX_MS + 1);  // 5 à 30 s
#if TRIGGER_DEBUG_SERIAL
      Serial.printf("[TRIGGER] Emotion OK (attente), prochaine dans %lu s\n", _nextIdleOkDelayMs / 1000);
#endif
    }
  }
}

void TriggerManager::checkTrigger(const String& triggerName) {
  if (!_initialized || !_enabled) {
    return;
  }

  if (!isCooldownElapsed()) {
#if TRIGGER_DEBUG_SERIAL
    Serial.println("[TRIGGER] Cooldown actif, trigger ignore");
#endif
    return;
  }

  activateTrigger(triggerName);
}

bool TriggerManager::evaluateTrigger(const String& triggerName) {
  // Obtenir les stats actuelles
  GotchiStats stats = LifeManager::getStats();

  // Hunger triggers — alignés sur le web (TriggerSelector.tsx) : faim ≤10%, ≤20%, 40–60%
  // stats.hunger = niveau de nourriture (100 = plein, 0 = affamé)
  if (triggerName == "hunger_critical") return stats.hunger <= 10;
  if (triggerName == "hunger_low") return stats.hunger <= 20 && stats.hunger > 10;
  if (triggerName == "hunger_medium") return stats.hunger >= 40 && stats.hunger <= 60;

  // Happiness triggers
  if (triggerName == "happiness_low") return stats.happiness <= 20;
  if (triggerName == "happiness_medium") return stats.happiness >= 40 && stats.happiness <= 60;
  if (triggerName == "happiness_high") return stats.happiness >= 80;

  // Health triggers
  if (triggerName == "health_critical") return stats.health <= 20;
  if (triggerName == "health_low") return stats.health <= 40 && stats.health > 20;
  if (triggerName == "health_good") return stats.health >= 80;

  // Fatigue triggers
  if (triggerName == "fatigue_high") return stats.fatigue >= 80;
  if (triggerName == "fatigue_low") return stats.fatigue <= 20;

  // Hygiene triggers
  if (triggerName == "hygiene_low") return stats.hygiene <= 20;
  if (triggerName == "hygiene_good") return stats.hygiene >= 80;

  // Eating : déclenché uniquement par LifeManager::checkTrigger("eating"), pas par la boucle stats
  if (triggerName == "eating") return false;

  // Manual or unknown
  return false;
}

bool TriggerManager::activateTrigger(const String& triggerName) {
  // Éviter de rejouer le même trigger avant la fin du cooldown (30 s)
  if (triggerName == _lastActiveTrigger && !isCooldownElapsed()) {
    return false;  // Skip, pas d'émotion jouée → laisser la place aux OK
  }

  // Pour les "demandes" (faim, gâteau, etc.) : exiger au moins N animations OK entre deux
  if (isDemandTrigger(triggerName) && _idleOkCountSinceLastDemand < MIN_OK_BETWEEN_DEMAND_TRIGGERS) {
    return false;  // Pas encore assez d'OK jouées → on jouera une OK à la place
  }

  if (_triggerIndex.find(triggerName) == _triggerIndex.end()) {
    if (s_loggedMissingTriggers.find(triggerName) == s_loggedMissingTriggers.end()) {
      s_loggedMissingTriggers.insert(triggerName);
      Serial.printf("[TRIGGER] Aucune emotion pour le trigger '%s' (message unique)\n", triggerName.c_str());
    }
    return false;
  }

  if (_triggerIndex[triggerName].empty()) {
    return false;
  }

  String emotionKey = selectRandomEmotion(triggerName);
  if (emotionKey.isEmpty()) {
    return false;
  }

#if TRIGGER_DEBUG_SERIAL
  Serial.printf("[TRIGGER] Activation du trigger '%s' -> emotion '%s'\n",
                triggerName.c_str(), emotionKey.c_str());
#endif

  EmotionPriority priority = EMOTION_PRIORITY_NORMAL;
  if (triggerName == "hunger_critical" || triggerName == "health_critical" || triggerName == "eating") {
    priority = EMOTION_PRIORITY_HIGH;
  }

  int variant = getRequestedVariant();
  if (EmotionManager::requestEmotion(emotionKey, 1, priority, variant, triggerName)) {
    _lastTriggerTime = millis();
    _lastActiveTrigger = triggerName;
    return true;
  }
  Serial.printf("[TRIGGER] Erreur: Impossible d'enqueuer l'emotion '%s'\n", emotionKey.c_str());
  return false;
}

String TriggerManager::selectRandomEmotion(const String& triggerName) {
  if (_triggerIndex.find(triggerName) == _triggerIndex.end()) {
    return "";
  }

  std::vector<IndexedEmotion>& emotions = _triggerIndex[triggerName];
  if (emotions.empty()) {
    return "";
  }

  // Pour "eating", utiliser le variant déjà défini (bottle=1, cake=2, apple=3, candy=4) par LifeManager
  // Pour les triggers de faim : variant 0 = accepter n'importe quel aliment (biberon, gâteau, etc.)
  int selectedVariant;
  bool acceptAnyFood = (triggerName == "hunger_critical" || triggerName == "hunger_low" || triggerName == "hunger_medium");

  if (triggerName == "eating" && _requestedVariant >= 1 && _requestedVariant <= 4) {
    selectedVariant = _requestedVariant;
  } else if (acceptAnyFood) {
    selectedVariant = 0;  // Quand il a faim, on accepte tout : NFC acceptera n'importe quel badge
  } else {
    selectedVariant = random(1, 5);  // random(1, 5) donne 1, 2, 3 ou 4
  }

  // Filtrer les émotions avec ce variant (variant 0 = prendre n'importe laquelle)
  std::vector<IndexedEmotion*> matchingEmotions;
  for (auto& emotion : emotions) {
    if (selectedVariant == 0 || emotion.variant == selectedVariant) {
      matchingEmotions.push_back(&emotion);
    }
  }

  // Si aucune émotion avec ce variant, prendre n'importe laquelle du trigger
  if (matchingEmotions.empty()) {
#if TRIGGER_DEBUG_SERIAL
    Serial.printf("[TRIGGER] Aucune emotion pour variant %d, selection aleatoire\n", selectedVariant);
#endif
    int randomIndex = random(0, emotions.size());
    IndexedEmotion* picked = &emotions[randomIndex];
    _requestedVariant = picked->variant;  // Variant du clip à jouer (bonne animation)
    return picked->key;
  }

  // Sélectionner aléatoirement parmi les émotions du variant choisi
  int randomIndex = random(0, matchingEmotions.size());
  IndexedEmotion* selected = matchingEmotions[randomIndex];
  _requestedVariant = selected->variant;  // Variant du clip à jouer (ex. 2 pour hunger_medium)
#if TRIGGER_DEBUG_SERIAL
  Serial.printf("[TRIGGER] Selection: variant %d -> %s\n", _requestedVariant, selected->key.c_str());
#endif
  return selected->key;
}

bool TriggerManager::isCooldownElapsed() {
  return (millis() - _lastTriggerTime) >= TRIGGER_COOLDOWN;
}

bool TriggerManager::isDemandTrigger(const String& triggerName) {
  // Triggers qui "demandent" quelque chose (faim, santé, etc.) → on espace par des OK
  return triggerName == "hunger_critical" || triggerName == "hunger_low" ||
         triggerName == "health_critical" || triggerName == "health_low" ||
         triggerName == "happiness_low" || triggerName == "fatigue_high" ||
         triggerName == "hygiene_low";
}

int TriggerManager::getEmotionCountForTrigger(const String& triggerName) {
  if (_triggerIndex.find(triggerName) == _triggerIndex.end()) {
    return 0;
  }
  return _triggerIndex[triggerName].size();
}

void TriggerManager::setEnabled(bool enabled) {
  _enabled = enabled;
  Serial.printf("[TRIGGER] Systeme %s\n", enabled ? "active" : "desactive");
}

bool TriggerManager::isEnabled() {
  return _enabled;
}

int TriggerManager::getRequestedVariant() {
  return _requestedVariant;
}

void TriggerManager::setRequestedVariant(int variant) {
  _requestedVariant = variant;
}

bool TriggerManager::isAcceptAnyFoodTrigger() {
  return _lastActiveTrigger == "hunger_critical" || _lastActiveTrigger == "hunger_low" ||
         _lastActiveTrigger == "hunger_medium";
}
