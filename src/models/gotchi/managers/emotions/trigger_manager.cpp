#include "trigger_manager.h"
#include "emotion_manager.h"
#include "../life/life_manager.h"
#include "../../../model_config.h"

#if defined(HAS_SD)
#include <SD.h>
#include <ArduinoJson.h>
#endif

// Constantes
#define TRIGGER_CHECK_INTERVAL 5000     // Vérifier les triggers toutes les 5 secondes
#define TRIGGER_COOLDOWN 30000          // 30 secondes entre deux triggers automatiques

// Variables statiques
bool TriggerManager::_initialized = false;
bool TriggerManager::_enabled = true;
unsigned long TriggerManager::_lastCheckTime = 0;
unsigned long TriggerManager::_lastTriggerTime = 0;
String TriggerManager::_lastActiveTrigger = "";
int TriggerManager::_requestedVariant = 0;
std::map<String, std::vector<IndexedEmotion>> TriggerManager::_triggerIndex;

bool TriggerManager::init() {
  _initialized = false;
  _enabled = true;
  _lastCheckTime = 0;
  _lastTriggerTime = 0;
  _lastActiveTrigger = "";
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

  StaticJsonDocument<512> configDoc;
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
  DynamicJsonDocument emotionsDoc(32768);
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

    Serial.printf("[TRIGGER] Indexe: %s -> %s (trigger: %s, variant: %d)\n",
                  key.c_str(), emotionId.c_str(), trigger.c_str(), variant);
  }

  Serial.printf("[TRIGGER] %d emotions indexees\n", emotionCount);
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

  // Vérifier si le cooldown global est écoulé
  if (!isCooldownElapsed()) {
    return;
  }

  // Nouveau: Ne pas évaluer les triggers si EmotionManager est déjà occupé
  if (EmotionManager::isPlaying()) {
    return;
  }

  // Parcourir tous les triggers et évaluer si l'un d'eux doit être activé
  // Pour éviter de spam, on active un seul trigger par cycle de vérification

  // Ordre de priorité des triggers (critiques en premier)
  const String priorityTriggers[] = {
    "hunger_critical",
    "health_critical",
    "hunger_low",
    "health_low",
    "happiness_low",
    "fatigue_high",
    "hygiene_low",
    "eating_finished",
    "hunger_full",
    "happiness_high",
    "health_good",
    "fatigue_low",
    "hygiene_good",
    "hunger_medium",
    "happiness_medium"
  };

  for (const String& triggerName : priorityTriggers) {
    if (evaluateTrigger(triggerName)) {
      // Ce trigger doit être activé !
      activateTrigger(triggerName);
      return; // Un seul trigger par cycle
    }
  }
}

void TriggerManager::checkTrigger(const String& triggerName) {
  if (!_initialized || !_enabled) {
    return;
  }

  if (!isCooldownElapsed()) {
    Serial.println("[TRIGGER] Cooldown actif, trigger ignore");
    return;
  }

  activateTrigger(triggerName);
}

bool TriggerManager::evaluateTrigger(const String& triggerName) {
  // Obtenir les stats actuelles
  GotchiStats stats = LifeManager::getStats();

  // Hunger triggers
  if (triggerName == "hunger_critical") return stats.hunger <= 10;
  if (triggerName == "hunger_low") return stats.hunger <= 20 && stats.hunger > 10;
  if (triggerName == "hunger_medium") return stats.hunger >= 40 && stats.hunger <= 60;
  if (triggerName == "hunger_full") return stats.hunger >= 90;

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

  // Manual or unknown
  return false;
}

void TriggerManager::activateTrigger(const String& triggerName) {
  // Éviter de rejouer le même trigger immédiatement
  if (triggerName == _lastActiveTrigger && !isCooldownElapsed()) {
    return;
  }

  // Vérifier qu'il y a des émotions pour ce trigger
  if (_triggerIndex.find(triggerName) == _triggerIndex.end()) {
    Serial.printf("[TRIGGER] Aucune emotion pour le trigger '%s'\n", triggerName.c_str());
    return;
  }

  if (_triggerIndex[triggerName].empty()) {
    return;
  }

  // Sélectionner aléatoirement une émotion
  String emotionKey = selectRandomEmotion(triggerName);
  if (emotionKey.isEmpty()) {
    return;
  }

  Serial.printf("[TRIGGER] Activation du trigger '%s' -> emotion '%s'\n",
                triggerName.c_str(), emotionKey.c_str());

  // Déterminer la priorité: triggers critiques = HIGH priority
  EmotionPriority priority = EMOTION_PRIORITY_NORMAL;
  if (triggerName == "hunger_critical" || triggerName == "health_critical") {
    priority = EMOTION_PRIORITY_HIGH;
  }

  // Demander la lecture de l'émotion (non-bloquant)
  if (EmotionManager::requestEmotion(emotionKey, 1, priority)) {
    // Mettre à jour les timestamps
    _lastTriggerTime = millis();
    _lastActiveTrigger = triggerName;
  } else {
    Serial.printf("[TRIGGER] Erreur: Impossible d'enqueuer l'emotion '%s'\n", emotionKey.c_str());
  }
}

String TriggerManager::selectRandomEmotion(const String& triggerName) {
  if (_triggerIndex.find(triggerName) == _triggerIndex.end()) {
    return "";
  }

  std::vector<IndexedEmotion>& emotions = _triggerIndex[triggerName];
  if (emotions.empty()) {
    return "";
  }

  // Sélectionner un variant aléatoire (1-4)
  int selectedVariant = random(1, 5);  // random(1, 5) donne 1, 2, 3 ou 4

  // Filtrer les émotions avec ce variant
  std::vector<IndexedEmotion*> matchingEmotions;
  for (auto& emotion : emotions) {
    if (emotion.variant == selectedVariant) {
      matchingEmotions.push_back(&emotion);
    }
  }

  // Si aucune émotion avec ce variant, prendre n'importe laquelle du trigger
  if (matchingEmotions.empty()) {
    Serial.printf("[TRIGGER] Aucune emotion pour variant %d, selection aleatoire\n", selectedVariant);
    int randomIndex = random(0, emotions.size());
    _requestedVariant = emotions[randomIndex].variant;
    return emotions[randomIndex].key;
  }

  // Sélectionner aléatoirement parmi les émotions du variant choisi
  int randomIndex = random(0, matchingEmotions.size());
  IndexedEmotion* selected = matchingEmotions[randomIndex];
  _requestedVariant = selected->variant;

  Serial.printf("[TRIGGER] Selection: variant %d -> %s\n", _requestedVariant, selected->key.c_str());
  return selected->key;
}

bool TriggerManager::isCooldownElapsed() {
  return (millis() - _lastTriggerTime) >= TRIGGER_COOLDOWN;
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
