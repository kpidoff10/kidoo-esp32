#include "wakeup_manager.h"
#include <ArduinoJson.h>
#include <limits.h>  // Pour ULONG_MAX
#include "../bedtime/bedtime_manager.h"

// Variables statiques
bool WakeupManager::initialized = false;
WakeupConfig WakeupManager::config;
WakeupConfig WakeupManager::lastConfig;
bool WakeupManager::wakeupActive = false;
unsigned long WakeupManager::wakeupStartTime = 0;
unsigned long WakeupManager::lastCheckTime = 0;
unsigned long WakeupManager::lastFadeUpdateTime = 0;
uint8_t WakeupManager::lastTriggeredHour = 255;  // 255 = jamais déclenché
uint8_t WakeupManager::lastTriggeredMinute = 255;
bool WakeupManager::checkingEnabled = false;
uint8_t WakeupManager::lastCheckedDay = 0;  // 0 = jamais vérifié
bool WakeupManager::fadeInActive = false;
bool WakeupManager::fadeOutActive = false;
unsigned long WakeupManager::fadeStartTime = 0;
uint8_t WakeupManager::startColorR = 0;
uint8_t WakeupManager::startColorG = 0;
uint8_t WakeupManager::startColorB = 0;
uint8_t WakeupManager::startBrightness = 0;
uint8_t WakeupManager::lastColorR = 255;
uint8_t WakeupManager::lastColorG = 255;
uint8_t WakeupManager::lastColorB = 255;
uint8_t WakeupManager::lastBrightness = 255;

// Constantes
static const unsigned long FADE_IN_DURATION_MS = 60000;      // 1 minute
static const unsigned long FADE_OUT_DURATION_MS = 300000;    // 5 minutes
static const unsigned long WAKEUP_DURATION_MS = 1800000;     // 30 minutes après l'heure de réveil avant fade-out
static const unsigned long CHECK_INTERVAL_MS = 60000;        // Vérifier toutes les minutes (quand proche de l'heure)
static const unsigned long CHECK_INTERVAL_3H_MS = 10800000;  // 3 heures (quand très loin)
static const unsigned long CHECK_INTERVAL_1H_MS = 3600000;   // 1 heure (quand loin)
static const unsigned long CHECK_INTERVAL_30M_MS = 1800000;  // 30 minutes (quand proche)
static const unsigned long FADE_UPDATE_INTERVAL_MS = 100;     // Mettre à jour le fade toutes les 100ms (10 fois par seconde)
static const int WAKEUP_TRIGGER_MINUTES_BEFORE = 15;         // Déclencher 15 minutes avant

bool WakeupManager::init() {
  if (initialized) {
    return true;
  }
  
  Serial.println("[WAKEUP] Initialisation du gestionnaire wake-up...");
  
  // Vérifier que le RTC est disponible
  if (!RTCManager::isAvailable()) {
    Serial.println("[WAKEUP] ERREUR: RTC non disponible");
    return false;
  }
  
  // Charger la configuration depuis la SD
  if (!loadConfig()) {
    Serial.println("[WAKEUP] ERREUR: Impossible de charger la configuration");
    return false;
  }
  
  initialized = true;
  
  // Initialiser l'état de vérification
  if (RTCManager::isAvailable()) {
    DateTime now = RTCManager::getDateTime();
    lastCheckedDay = now.dayOfWeek;
    updateCheckingState();
    
    // Initialiser lastCheckTime pour démarrer avec le bon intervalle
    lastCheckTime = millis();
    
    // Afficher l'intervalle de vérification calculé
    if (checkingEnabled) {
      unsigned long interval = calculateNextCheckInterval();
      Serial.printf("[WAKEUP] Intervalle de verification initial: %lu ms (%.1f heures)\n",
                    interval, interval / 3600000.0f);
    }
  } else {
    checkingEnabled = false;
    lastCheckedDay = 0;
    lastCheckTime = millis();
  }
  
  Serial.println("[WAKEUP] Gestionnaire initialise");
  
  return true;
}

bool WakeupManager::loadConfig() {
  // Sauvegarder l'ancienne config pour détecter les changements
  lastConfig = config;
  
  // Charger la configuration depuis la SD
  SDConfig sdConfig = SDManager::getConfig();
  
  // Copier les paramètres généraux
  config.colorR = sdConfig.wakeup_colorR;
  config.colorG = sdConfig.wakeup_colorG;
  config.colorB = sdConfig.wakeup_colorB;
  config.brightness = sdConfig.wakeup_brightness;
  
  // Initialiser tous les schedules à désactivés par défaut
  for (int i = 0; i < 7; i++) {
    config.schedules[i].hour = 7;
    config.schedules[i].minute = 0;
    config.schedules[i].activated = false;
  }
  
  // Parser le weekdaySchedule JSON
  if (strlen(sdConfig.wakeup_weekdaySchedule) > 0) {
    parseWeekdaySchedule(sdConfig.wakeup_weekdaySchedule);
  }
  
  // Charger la couleur de coucher depuis la config bedtime
  loadBedtimeColor();
  
  Serial.println("[WAKEUP] Configuration chargee depuis la SD");
  Serial.printf("[WAKEUP] Couleur RGB(%d, %d, %d), Brightness: %d%%\n",
                config.colorR, config.colorG, config.colorB, config.brightness);
  Serial.printf("[WAKEUP] Couleur de depart (bedtime) RGB(%d, %d, %d)\n",
                startColorR, startColorG, startColorB);
  
  return true;
}

void WakeupManager::loadBedtimeColor() {
  // Charger la couleur de coucher depuis la config bedtime
  BedtimeConfig bedtimeConfig = BedtimeManager::getConfig();
  startColorR = bedtimeConfig.colorR;
  startColorG = bedtimeConfig.colorG;
  startColorB = bedtimeConfig.colorB;
  
  Serial.printf("[WAKEUP] Couleur bedtime chargee: RGB(%d, %d, %d)\n",
                startColorR, startColorG, startColorB);
}

bool WakeupManager::reloadConfig() {
  Serial.println("[WAKEUP] Rechargement de la configuration...");
  
  // Réinitialiser les flags de déclenchement pour permettre un nouveau déclenchement
  lastTriggeredHour = 255;
  lastTriggeredMinute = 255;
  
  bool result = loadConfig();
  
  // Si la config a changé, vérifier si la routine est activée pour aujourd'hui
  if (result && initialized && RTCManager::isAvailable()) {
    if (configChanged()) {
      Serial.println("[WAKEUP] Configuration modifiee, verification de l'etat pour aujourd'hui");
      updateCheckingState();
      
      // Réinitialiser lastCheckTime pour recalculer l'intervalle avec la nouvelle config
      lastCheckTime = millis();
      
      // Afficher le nouvel intervalle de vérification
      if (checkingEnabled) {
        unsigned long interval = calculateNextCheckInterval();
        Serial.printf("[WAKEUP] Nouvel intervalle de verification: %lu ms (%.1f heures)\n",
                      interval, interval / 3600000.0f);
      }
      
      // Si maintenant activé pour aujourd'hui, vérifier immédiatement
      if (checkingEnabled) {
        checkNow();
      }
    } else {
      // Config identique, juste vérifier maintenant si déjà en cours de vérification
      if (checkingEnabled) {
        // Réinitialiser quand même lastCheckTime pour recalculer l'intervalle (au cas où l'heure aurait changé)
        lastCheckTime = millis();
        checkNow();
      }
    }
  }
  
  return result;
}

void WakeupManager::checkNow() {
  if (!initialized || !RTCManager::isAvailable()) {
    return;
  }
  
  Serial.println("[WAKEUP] Vérification immédiate après mise à jour de la configuration");
  
  // Vérifier immédiatement si c'est l'heure de déclencher le wake-up
  checkWakeupTrigger();
}

void WakeupManager::parseWeekdaySchedule(const char* jsonStr) {
  if (!jsonStr || strlen(jsonStr) == 0) {
    return;
  }
  
  // Parser le JSON
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  StaticJsonDocument<512> doc;
  #pragma GCC diagnostic pop
  
  DeserializationError error = deserializeJson(doc, jsonStr);
  if (error) {
    Serial.print("[WAKEUP] Erreur parsing weekdaySchedule: ");
    Serial.println(error.c_str());
    return;
  }
  
  // Mapping des jours de la semaine
  const char* weekdays[] = {"monday", "tuesday", "wednesday", "thursday", "friday", "saturday", "sunday"};
  
  // Parser chaque jour
  for (int i = 0; i < 7; i++) {
    if (doc[weekdays[i]].is<JsonObject>()) {
      JsonObject daySchedule = doc[weekdays[i]].as<JsonObject>();
      
      if (daySchedule["hour"].is<int>()) {
        config.schedules[i].hour = daySchedule["hour"].as<int>();
      }
      if (daySchedule["minute"].is<int>()) {
        config.schedules[i].minute = daySchedule["minute"].as<int>();
      }
      if (daySchedule["activated"].is<bool>()) {
        config.schedules[i].activated = daySchedule["activated"].as<bool>();
      } else {
        // Si activated n'est pas présent, considérer comme activé si hour/minute sont présents
        config.schedules[i].activated = daySchedule["hour"].is<int>() && daySchedule["minute"].is<int>();
      }
      
      if (config.schedules[i].activated) {
        Serial.printf("[WAKEUP] %s: %02d:%02d (active)\n", weekdays[i], 
                      config.schedules[i].hour, config.schedules[i].minute);
      }
    }
  }
}

uint8_t WakeupManager::weekdayToIndex(uint8_t dayOfWeek) {
  // RTC dayOfWeek: 1=Lundi, 7=Dimanche
  // Notre index: 0=Lundi, 6=Dimanche
  if (dayOfWeek >= 1 && dayOfWeek <= 7) {
    return dayOfWeek - 1;
  }
  return 0; // Par défaut, lundi
}

const char* WakeupManager::indexToWeekday(uint8_t index) {
  const char* weekdays[] = {"monday", "tuesday", "wednesday", "thursday", "friday", "saturday", "sunday"};
  if (index < 7) {
    return weekdays[index];
  }
  return weekdays[0];
}

void WakeupManager::update() {
  if (!initialized) {
    return;
  }
  
  if (!RTCManager::isAvailable()) {
    // Log seulement toutes les 5 minutes pour éviter le spam
    static unsigned long lastRtcErrorLog = 0;
    unsigned long currentTime = millis();
    if (currentTime - lastRtcErrorLog >= 300000) { // 5 minutes
      Serial.println("[WAKEUP] ERREUR: RTC non disponible, impossible de vérifier l'heure");
      lastRtcErrorLog = currentTime;
    }
    return;
  }
  
  DateTime now = RTCManager::getDateTime();
  
  // Vérifier si le jour a changé
  if (lastCheckedDay != now.dayOfWeek) {
    Serial.printf("[WAKEUP] Changement de jour detecte: %d -> %d\n", lastCheckedDay, now.dayOfWeek);
    lastCheckedDay = now.dayOfWeek;
    updateCheckingState();  // Mettre à jour l'état de vérification pour le nouveau jour
  }
  
  // Ne vérifier que si la routine est activée pour aujourd'hui
  if (!checkingEnabled) {
    return;  // Routine non activée pour aujourd'hui, pas besoin de vérifier
  }
  
  unsigned long currentTime = millis();
  
  // Calculer le prochain intervalle de vérification basé sur la distance jusqu'à l'heure de déclenchement
  unsigned long nextCheckInterval = calculateNextCheckInterval();
  
  // Vérifier périodiquement avec un intervalle adaptatif
  unsigned long elapsed;
  if (currentTime >= lastCheckTime) {
    elapsed = currentTime - lastCheckTime;
  } else {
    elapsed = (ULONG_MAX - lastCheckTime) + currentTime + 1;
  }
  
  if (elapsed >= nextCheckInterval) {
    lastCheckTime = currentTime;
    checkWakeupTrigger();
  }
  
  // Mettre à jour les animations de fade si actives (avec throttling pour éviter les appels trop fréquents)
  if (fadeInActive) {
    unsigned long timeSinceLastFadeUpdate;
    if (currentTime >= lastFadeUpdateTime) {
      timeSinceLastFadeUpdate = currentTime - lastFadeUpdateTime;
    } else {
      timeSinceLastFadeUpdate = (ULONG_MAX - lastFadeUpdateTime) + currentTime;
    }
    
    if (timeSinceLastFadeUpdate >= FADE_UPDATE_INTERVAL_MS) {
      lastFadeUpdateTime = currentTime;
      updateFadeIn();
    }
  }
  
  // Vérifier si on doit démarrer le fade-out (30 minutes après l'heure de réveil exacte)
  if (wakeupActive && !fadeInActive && !fadeOutActive) {
    // Calculer le temps écoulé depuis le début du wake-up
    unsigned long elapsedSinceStart;
    
    // Gérer le wrap-around de millis() (se produit après ~49 jours)
    if (currentTime >= wakeupStartTime) {
      elapsedSinceStart = currentTime - wakeupStartTime;
    } else {
      // Wrap-around détecté
      elapsedSinceStart = (ULONG_MAX - wakeupStartTime) + currentTime;
    }
    
    // Le fade-in dure 15 minutes, donc après 15 + 30 = 45 minutes depuis le début
    // on démarre le fade-out (30 minutes après l'heure de réveil exacte)
    unsigned long wakeupDurationWithFadeIn = FADE_IN_DURATION_MS + WAKEUP_DURATION_MS;
    
    if (elapsedSinceStart >= wakeupDurationWithFadeIn) {
      // Démarrer le fade-out après 30 minutes après l'heure de réveil
      fadeOutActive = true;
      fadeStartTime = currentTime;
      Serial.println("[WAKEUP] 30 minutes après l'heure de réveil écoulées, démarrage du fade-out (5 minutes de fade-out)");
    }
  }
  
  // Mettre à jour le fade-out si actif (avec throttling)
  if (fadeOutActive) {
    unsigned long timeSinceLastFadeUpdate;
    if (currentTime >= lastFadeUpdateTime) {
      timeSinceLastFadeUpdate = currentTime - lastFadeUpdateTime;
    } else {
      timeSinceLastFadeUpdate = (ULONG_MAX - lastFadeUpdateTime) + currentTime;
    }
    
    if (timeSinceLastFadeUpdate >= FADE_UPDATE_INTERVAL_MS) {
      lastFadeUpdateTime = currentTime;
      updateFadeOut();
    }
  }
}

void WakeupManager::updateCheckingState() {
  if (!RTCManager::isAvailable()) {
    checkingEnabled = false;
    return;
  }
  
  DateTime now = RTCManager::getDateTime();
  uint8_t dayIndex = weekdayToIndex(now.dayOfWeek);
  
  // Vérifier si la routine est activée pour aujourd'hui
  bool wasEnabled = checkingEnabled;
  checkingEnabled = config.schedules[dayIndex].activated;
  
  if (checkingEnabled) {
    if (!wasEnabled) {
      // Réinitialiser lastCheckTime quand on active la vérification
      lastCheckTime = millis();
    }
    unsigned long interval = calculateNextCheckInterval();
    Serial.printf("[WAKEUP] Routine activee pour aujourd'hui (%s), verification adaptative activee (intervalle: %lu ms = %.1f heures)\n",
                  indexToWeekday(dayIndex), interval, interval / 3600000.0f);
  } else {
    Serial.printf("[WAKEUP] Routine non activee pour aujourd'hui (%s), verification desactivee jusqu'au jour suivant\n",
                  indexToWeekday(dayIndex));
  }
}

bool WakeupManager::configChanged() {
  // Comparer les schedules (les plus importants pour l'optimisation)
  for (int i = 0; i < 7; i++) {
    if (config.schedules[i].hour != lastConfig.schedules[i].hour ||
        config.schedules[i].minute != lastConfig.schedules[i].minute ||
        config.schedules[i].activated != lastConfig.schedules[i].activated) {
      return true;
    }
  }
  
  // Comparer aussi les autres paramètres (au cas où)
  if (config.colorR != lastConfig.colorR ||
      config.colorG != lastConfig.colorG ||
      config.colorB != lastConfig.colorB ||
      config.brightness != lastConfig.brightness) {
    return true;
  }
  
  return false;
}

unsigned long WakeupManager::calculateNextCheckInterval() {
  if (!RTCManager::isAvailable()) {
    return CHECK_INTERVAL_MS;  // Par défaut, toutes les minutes
  }
  
  DateTime now = RTCManager::getDateTime();
  uint8_t dayIndex = weekdayToIndex(now.dayOfWeek);
  
  if (!config.schedules[dayIndex].activated) {
    return CHECK_INTERVAL_3H_MS;  // Non activé, vérifier toutes les 3h au cas où
  }
  
  // Calculer l'heure de déclenchement (15 minutes avant l'heure de réveil)
  int targetHour = config.schedules[dayIndex].hour;
  int targetMinute = config.schedules[dayIndex].minute - WAKEUP_TRIGGER_MINUTES_BEFORE;
  
  // Gérer le débordement si targetMinute < 0
  if (targetMinute < 0) {
    targetMinute += 60;
    targetHour -= 1;
    if (targetHour < 0) {
      targetHour += 24;
    }
  }
  
  // Calculer les minutes jusqu'à l'heure de déclenchement
  int currentMinutes = now.hour * 60 + now.minute;
  int triggerMinutes = targetHour * 60 + targetMinute;
  int minutesUntilTarget = triggerMinutes - currentMinutes;
  
  // Si l'heure de déclenchement est passée aujourd'hui, c'est pour demain
  if (minutesUntilTarget < 0) {
    minutesUntilTarget += 24 * 60;  // Ajouter 24 heures
  }
  
  // Convertir en heures
  float hoursUntilTarget = minutesUntilTarget / 60.0f;
  
  // Déterminer l'intervalle de vérification basé sur la distance
  if (hoursUntilTarget > 6.0f) {
    // Plus de 6 heures avant : vérifier toutes les 3 heures
    return CHECK_INTERVAL_3H_MS;
  } else if (hoursUntilTarget > 3.0f) {
    // 3-6 heures avant : vérifier toutes les heures
    return CHECK_INTERVAL_1H_MS;
  } else if (hoursUntilTarget > 1.0f) {
    // 1-3 heures avant : vérifier toutes les 30 minutes
    return CHECK_INTERVAL_30M_MS;
  } else {
    // Moins d'1 heure avant : vérifier toutes les minutes
    return CHECK_INTERVAL_MS;
  }
}

void WakeupManager::checkWakeupTrigger() {
  DateTime now = RTCManager::getDateTime();
  uint8_t dayIndex = weekdayToIndex(now.dayOfWeek);
  
  // Log de débogage pour chaque vérification
  Serial.printf("[WAKEUP] Vérification: Heure actuelle %02d:%02d:%02d, Jour de la semaine: %d (index: %d)\n",
                now.hour, now.minute, now.second, now.dayOfWeek, dayIndex);
  Serial.printf("[WAKEUP] Configuration pour ce jour: %02d:%02d, Activé: %s\n",
                config.schedules[dayIndex].hour, config.schedules[dayIndex].minute,
                config.schedules[dayIndex].activated ? "Oui" : "Non");
  
  // Vérifier si le wake-up est activé pour aujourd'hui
  if (!config.schedules[dayIndex].activated) {
    Serial.println("[WAKEUP] Le wake-up n'est pas activé pour aujourd'hui");
    // Si le wake-up était actif mais le jour n'est plus activé, l'arrêter
    if (wakeupActive) {
      Serial.println("[WAKEUP] Arrêt du wake-up car le jour n'est plus activé");
      stopWakeup();
    }
    return;
  }
  
  // Calculer l'heure de déclenchement (15 minutes avant l'heure de réveil)
  int triggerHour = config.schedules[dayIndex].hour;
  int triggerMinute = config.schedules[dayIndex].minute - WAKEUP_TRIGGER_MINUTES_BEFORE;
  
  // Gérer le débordement si triggerMinute < 0
  if (triggerMinute < 0) {
    triggerMinute += 60;
    triggerHour -= 1;
    if (triggerHour < 0) {
      triggerHour += 24;
    }
  }
  
  // Vérifier si c'est l'heure de déclenchement (dans la minute, secondes 0-59)
  if (now.hour == triggerHour && now.minute == triggerMinute) {
    Serial.printf("[WAKEUP] Heure correspondante détectée! Wake-up actif: %s, Last triggered: %02d:%02d\n",
                  wakeupActive ? "Oui" : "Non", lastTriggeredHour, lastTriggeredMinute);
    
    // Déclencher le wake-up si pas déjà actif et qu'on n'a pas déjà déclenché cette minute
    if (!wakeupActive && 
        (lastTriggeredHour != now.hour || lastTriggeredMinute != now.minute)) {
      Serial.println("[WAKEUP] >>> DÉCLENCHEMENT DU WAKE-UP <<<");
      startWakeup();
      lastTriggeredHour = now.hour;
      lastTriggeredMinute = now.minute;
    } else {
      if (wakeupActive) {
        Serial.println("[WAKEUP] Wake-up déjà actif, pas de nouveau déclenchement");
      } else {
        Serial.println("[WAKEUP] Déjà déclenché cette minute, pas de nouveau déclenchement");
      }
    }
  } else {
    // Log pour comprendre pourquoi ça ne correspond pas
    Serial.printf("[WAKEUP] Heure ne correspond pas: Actuelle %02d:%02d vs Trigger %02d:%02d (réveil à %02d:%02d)\n",
                  now.hour, now.minute, triggerHour, triggerMinute,
                  config.schedules[dayIndex].hour, config.schedules[dayIndex].minute);
    
    // Si on n'est plus dans la minute de déclenchement, réinitialiser les flags de déclenchement
    if (lastTriggeredHour == triggerHour && 
        lastTriggeredMinute == triggerMinute) {
      // On est sorti de la minute de déclenchement, réinitialiser pour permettre un nouveau déclenchement demain
      Serial.println("[WAKEUP] Sortie de la minute de déclenchement, réinitialisation des flags");
      lastTriggeredHour = 255;
      lastTriggeredMinute = 255;
    }
  }
}

void WakeupManager::startWakeup() {
  Serial.println("[WAKEUP] Démarrage du wake-up automatique");
  
  wakeupActive = true;
  wakeupStartTime = millis();
  fadeInActive = true;
  fadeOutActive = false;
  fadeStartTime = millis();
  
  // Recharger la couleur de coucher au cas où elle aurait changé
  loadBedtimeColor();
  
  // Empêcher le sleep mode pendant le wake-up
  LEDManager::preventSleep();
  
  // Réveiller les LEDs
  LEDManager::wakeUp();
  
  // Désactiver les effets animés
  LEDManager::setEffect(LED_EFFECT_NONE);
  
  // Récupérer la brightness actuelle des LEDs (ne pas repartir de 0)
  startBrightness = LEDManager::getCurrentBrightness();
  
  // Commencer avec la couleur de coucher (brightness sera gérée par le fade-in)
  LEDManager::setColor(startColorR, startColorG, startColorB);
  
  // Initialiser les dernières valeurs pour éviter les appels répétés
  lastColorR = startColorR;
  lastColorG = startColorG;
  lastColorB = startColorB;
  lastBrightness = startBrightness;
  
  Serial.printf("[WAKEUP] Couleur de depart RGB(%d, %d, %d), Couleur cible RGB(%d, %d, %d)\n",
                startColorR, startColorG, startColorB,
                config.colorR, config.colorG, config.colorB);
  Serial.printf("[WAKEUP] Brightness de depart: %d (0-255), Brightness cible: %d%% (%d)\n",
                startBrightness, config.brightness, (config.brightness * 255 + 50) / 100);
}

void WakeupManager::updateFadeIn() {
  unsigned long elapsed;
  unsigned long currentTime = millis();
  
  // Gérer le wrap-around de millis()
  if (currentTime >= fadeStartTime) {
    elapsed = currentTime - fadeStartTime;
  } else {
    elapsed = (ULONG_MAX - fadeStartTime) + currentTime;
  }
  
  if (elapsed >= FADE_IN_DURATION_MS) {
    // Fade-in terminé
    fadeInActive = false;
    
    // Appliquer la couleur et brightness finales seulement si elles ont changé
    if (lastColorR != config.colorR || lastColorG != config.colorG || lastColorB != config.colorB) {
      LEDManager::setColor(config.colorR, config.colorG, config.colorB);
      lastColorR = config.colorR;
      lastColorG = config.colorG;
      lastColorB = config.colorB;
    }
    
    uint8_t brightnessValue = (config.brightness * 255 + 50) / 100;
    if (lastBrightness != brightnessValue) {
      LEDManager::setBrightness(brightnessValue);
      lastBrightness = brightnessValue;
    }
    
    Serial.println("[WAKEUP] Fade-in termine");
  } else {
    // Interpolation linéaire de la brightness et de la couleur
    float progress = (float)elapsed / (float)FADE_IN_DURATION_MS;
    
    // Brightness: startBrightness → targetBrightness (ne pas repartir de 0)
    uint8_t targetBrightness = (config.brightness * 255 + 50) / 100;
    uint8_t currentBrightness = (uint8_t)(startBrightness + (targetBrightness - startBrightness) * progress);
    
    // Couleur: interpolation linéaire RGB de startColor vers config.color
    uint8_t currentR = (uint8_t)(startColorR + (config.colorR - startColorR) * progress);
    uint8_t currentG = (uint8_t)(startColorG + (config.colorG - startColorG) * progress);
    uint8_t currentB = (uint8_t)(startColorB + (config.colorB - startColorB) * progress);
    
    // Ne mettre à jour que si la couleur ou brightness a changé
    if (lastColorR != currentR || lastColorG != currentG || lastColorB != currentB) {
      LEDManager::setColor(currentR, currentG, currentB);
      lastColorR = currentR;
      lastColorG = currentG;
      lastColorB = currentB;
    }
    
    if (lastBrightness != currentBrightness) {
      LEDManager::setBrightness(currentBrightness);
      lastBrightness = currentBrightness;
    }
  }
}

void WakeupManager::updateFadeOut() {
  unsigned long elapsed;
  unsigned long currentTime = millis();
  
  // Gérer le wrap-around de millis()
  if (currentTime >= fadeStartTime) {
    elapsed = currentTime - fadeStartTime;
  } else {
    elapsed = (ULONG_MAX - fadeStartTime) + currentTime;
  }
  
  if (elapsed >= FADE_OUT_DURATION_MS) {
    // Fade-out terminé, éteindre complètement et arrêter le wake-up
    fadeOutActive = false;
    LEDManager::clear();
    wakeupActive = false; // Arrêter le wake-up après le fade-out
    Serial.println("[WAKEUP] Fade-out termine, LEDs eteintes, wake-up arrete");
  } else {
    // Interpolation linéaire de la brightness vers 0
    float progress = (float)elapsed / (float)FADE_OUT_DURATION_MS;
    uint8_t startBrightness = (config.brightness * 255 + 50) / 100;
    uint8_t currentBrightness = (uint8_t)(startBrightness * (1.0f - progress));
    
    LEDManager::setBrightness(currentBrightness);
  }
}

void WakeupManager::stopWakeup() {
  Serial.println("[WAKEUP] Arrêt du wake-up");
  
  wakeupActive = false;
  fadeInActive = false;
  fadeOutActive = false;
  
  // Réautoriser le sleep mode
  LEDManager::allowSleep();
  
  // Éteindre les LEDs
  LEDManager::clear();
}

bool WakeupManager::isWakeupEnabled() {
  if (!initialized) {
    return false;
  }
  
  DateTime now = RTCManager::getDateTime();
  uint8_t dayIndex = weekdayToIndex(now.dayOfWeek);
  
  return config.schedules[dayIndex].activated;
}

WakeupConfig WakeupManager::getConfig() {
  return config;
}

bool WakeupManager::isWakeupActive() {
  return wakeupActive;
}

void WakeupManager::stopWakeupManually() {
  Serial.println("[WAKEUP] Arrêt manuel du wake-up");
  
  // Arrêter le wake-up (qui réinitialisera aussi les états)
  stopWakeup();
}
