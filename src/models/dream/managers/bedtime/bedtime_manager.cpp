#include "bedtime_manager.h"
#include <ArduinoJson.h>
#include <limits.h>  // Pour ULONG_MAX

// Variables statiques
bool BedtimeManager::initialized = false;
BedtimeConfig BedtimeManager::config;
BedtimeConfig BedtimeManager::lastConfig;
bool BedtimeManager::bedtimeActive = false;
bool BedtimeManager::manuallyStarted = false;
unsigned long BedtimeManager::bedtimeStartTime = 0;
unsigned long BedtimeManager::lastCheckTime = 0;
uint8_t BedtimeManager::lastTriggeredHour = 255;  // 255 = jamais déclenché
uint8_t BedtimeManager::lastTriggeredMinute = 255;
bool BedtimeManager::checkingEnabled = false;
uint8_t BedtimeManager::lastCheckedDay = 0;  // 0 = jamais vérifié
bool BedtimeManager::fadeInActive = false;
bool BedtimeManager::fadeOutActive = false;
unsigned long BedtimeManager::fadeStartTime = 0;

// Constantes
static const unsigned long FADE_IN_DURATION_MS = 30000;      // 30 secondes
static const unsigned long FADE_OUT_DURATION_MS = 300000;    // 5 minutes
static const unsigned long BEDTIME_DURATION_MS = 1800000;     // 30 minutes avant fade-out
static const unsigned long CHECK_INTERVAL_MS = 60000;        // Vérifier toutes les minutes (quand proche de l'heure)
static const unsigned long CHECK_INTERVAL_3H_MS = 10800000;  // 3 heures (quand très loin)
static const unsigned long CHECK_INTERVAL_1H_MS = 3600000;   // 1 heure (quand loin)
static const unsigned long CHECK_INTERVAL_30M_MS = 1800000;  // 30 minutes (quand proche)

bool BedtimeManager::init() {
  if (initialized) {
    return true;
  }
  
  Serial.println("[BEDTIME] Initialisation du gestionnaire bedtime...");
  
  // Vérifier que le RTC est disponible
  if (!RTCManager::isAvailable()) {
    Serial.println("[BEDTIME] ERREUR: RTC non disponible");
    return false;
  }
  
  // Charger la configuration depuis la SD
  if (!loadConfig()) {
    Serial.println("[BEDTIME] ERREUR: Impossible de charger la configuration");
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
    
    // Au démarrage : si on est dans la plage nuit, activer soit bedtime soit laisser WakeupManager démarrer le wakeup
    // (dans la fenêtre 15 min avant lever → 35 min après = mode wakeup, sinon mode bedtime)
    if (checkingEnabled) {
      uint8_t dayIndex = weekdayToIndex(now.dayOfWeek);
      int wakeupHour = 7, wakeupMinute = 0;
      bool hasWakeup = getWakeupScheduleForDay(dayIndex, wakeupHour, wakeupMinute);
      if (hasWakeup && isCurrentTimeBetweenBedtimeAndWakeup(dayIndex, now.hour, now.minute, wakeupHour, wakeupMinute)) {
        if (!isCurrentTimeInWakeupWindow(now.hour, now.minute, wakeupHour, wakeupMinute)) {
          Serial.println("[BEDTIME] Demarrage: heure dans la plage coucher->lever (hors fenetre wakeup), activation routine bedtime");
          startBedtime();
          fadeInActive = false;  // Pas de fade-in au boot, affichage direct
          uint8_t brightnessValue = (config.brightness * 255 + 50) / 100;
          LEDManager::setBrightness(brightnessValue);
          lastTriggeredHour = config.schedules[dayIndex].hour;
          lastTriggeredMinute = config.schedules[dayIndex].minute;
        } else {
          Serial.println("[BEDTIME] Demarrage: dans la fenetre wakeup (15 min avant lever -> 35 min apres), routine wakeup sera demarree");
        }
      }
    }
    
    // Afficher l'intervalle de vérification calculé
    if (checkingEnabled && !bedtimeActive) {
      unsigned long interval = calculateNextCheckInterval();
      Serial.printf("[BEDTIME] Intervalle de verification initial: %lu ms (%.1f heures)\n",
                    interval, interval / 3600000.0f);
    }
  } else {
    checkingEnabled = false;
    lastCheckedDay = 0;
    lastCheckTime = millis();
  }
  
  Serial.println("[BEDTIME] Gestionnaire initialise");
  
  return true;
}

bool BedtimeManager::loadConfig() {
  // Sauvegarder l'ancienne config pour détecter les changements
  lastConfig = config;
  
  // Charger la configuration depuis la SD
  SDConfig sdConfig = SDManager::getConfig();
  
  // Copier les paramètres généraux
  config.colorR = sdConfig.bedtime_colorR;
  config.colorG = sdConfig.bedtime_colorG;
  config.colorB = sdConfig.bedtime_colorB;
  config.brightness = sdConfig.bedtime_brightness;
  config.allNight = sdConfig.bedtime_allNight;
  
  // Copier l'effet (ou "none" si vide)
  if (strlen(sdConfig.bedtime_effect) > 0) {
    strncpy(config.effect, sdConfig.bedtime_effect, sizeof(config.effect) - 1);
    config.effect[sizeof(config.effect) - 1] = '\0';
  } else {
    strcpy(config.effect, "none");
  }
  
  // Initialiser tous les schedules à désactivés par défaut
  for (int i = 0; i < 7; i++) {
    config.schedules[i].hour = 20;
    config.schedules[i].minute = 0;
    config.schedules[i].activated = false;
  }
  
  // Parser le weekdaySchedule JSON
  if (strlen(sdConfig.bedtime_weekdaySchedule) > 0) {
    parseWeekdaySchedule(sdConfig.bedtime_weekdaySchedule);
  }
  
  Serial.println("[BEDTIME] Configuration chargee depuis la SD");
  Serial.printf("[BEDTIME] Couleur RGB(%d, %d, %d), Brightness: %d%%, AllNight: %s, Effect: %s\n",
                config.colorR, config.colorG, config.colorB, config.brightness,
                config.allNight ? "true" : "false", config.effect);
  
  return true;
}

bool BedtimeManager::reloadConfig() {
  Serial.println("[BEDTIME] Rechargement de la configuration...");
  
  // Réinitialiser les flags de déclenchement pour permettre un nouveau déclenchement
  lastTriggeredHour = 255;
  lastTriggeredMinute = 255;
  
  bool result = loadConfig();
  
  // Si la config a changé, vérifier si la routine est activée pour aujourd'hui
  if (result && initialized && RTCManager::isAvailable()) {
    if (configChanged()) {
      Serial.println("[BEDTIME] Configuration modifiee, verification de l'etat pour aujourd'hui");
      updateCheckingState();
      
      // Réinitialiser lastCheckTime pour recalculer l'intervalle avec la nouvelle config
      lastCheckTime = millis();
      
      // Afficher le nouvel intervalle de vérification
      if (checkingEnabled) {
        unsigned long interval = calculateNextCheckInterval();
        Serial.printf("[BEDTIME] Nouvel intervalle de verification: %lu ms (%.1f heures)\n",
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

void BedtimeManager::checkNow() {
  if (!initialized || !RTCManager::isAvailable()) {
    return;
  }
  
  Serial.println("[BEDTIME] Vérification immédiate après mise à jour de la configuration");
  
  // Vérifier immédiatement si c'est l'heure de déclencher le bedtime
  checkBedtimeTrigger();
}

void BedtimeManager::parseWeekdaySchedule(const char* jsonStr) {
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
    Serial.print("[BEDTIME] Erreur parsing weekdaySchedule: ");
    Serial.println(error.c_str());
    return;
  }
  
  // Mapping des jours de la semaine
  const char* weekdays[] = {"monday", "tuesday", "wednesday", "thursday", "friday", "saturday", "sunday"};
  
  // Parser chaque jour (accepter hour/minute en int ou double pour compatibilité JSON)
  for (int i = 0; i < 7; i++) {
    if (doc[weekdays[i]].is<JsonObject>()) {
      JsonObject daySchedule = doc[weekdays[i]].as<JsonObject>();
      int h = -1, m = -1;
      if (daySchedule["hour"].is<int>()) h = daySchedule["hour"].as<int>();
      else if (daySchedule["hour"].is<double>()) h = (int)daySchedule["hour"].as<double>();
      if (daySchedule["minute"].is<int>()) m = daySchedule["minute"].as<int>();
      else if (daySchedule["minute"].is<double>()) m = (int)daySchedule["minute"].as<double>();
      if (h >= 0 && h <= 23) config.schedules[i].hour = (uint8_t)h;
      if (m >= 0 && m <= 59) config.schedules[i].minute = (uint8_t)m;
      if (daySchedule["activated"].is<bool>()) {
        config.schedules[i].activated = daySchedule["activated"].as<bool>();
      } else {
        config.schedules[i].activated = (h >= 0 && m >= 0);
      }
      if (config.schedules[i].activated) {
        Serial.printf("[BEDTIME] %s: %02d:%02d (active)\n", weekdays[i],
                      config.schedules[i].hour, config.schedules[i].minute);
      }
    }
  }
}

uint8_t BedtimeManager::weekdayToIndex(uint8_t dayOfWeek) {
  // RTC dayOfWeek: 1=Lundi, 7=Dimanche
  // Notre index: 0=Lundi, 6=Dimanche
  if (dayOfWeek >= 1 && dayOfWeek <= 7) {
    return dayOfWeek - 1;
  }
  return 0; // Par défaut, lundi
}

bool BedtimeManager::getWakeupScheduleForDay(uint8_t dayIndex, int& outHour, int& outMinute) {
  if (dayIndex >= 7) {
    return false;
  }
  SDConfig sdConfig = SDManager::getConfig();
  if (!sdConfig.wakeup_weekdaySchedule || strlen(sdConfig.wakeup_weekdaySchedule) == 0) {
    return false;
  }
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  StaticJsonDocument<512> doc;
  #pragma GCC diagnostic pop
  DeserializationError error = deserializeJson(doc, sdConfig.wakeup_weekdaySchedule);
  if (error) {
    return false;
  }
  const char* weekdays[] = {"monday", "tuesday", "wednesday", "thursday", "friday", "saturday", "sunday"};
  if (!doc[weekdays[dayIndex]].is<JsonObject>()) {
    return false;
  }
  JsonObject daySchedule = doc[weekdays[dayIndex]].as<JsonObject>();
  if (!daySchedule["hour"].is<int>() || !daySchedule["minute"].is<int>()) {
    return false;
  }
  outHour = daySchedule["hour"].as<int>();
  outMinute = daySchedule["minute"].as<int>();
  return true;
}

bool BedtimeManager::isCurrentTimeBetweenBedtimeAndWakeup(uint8_t dayIndex, int nowHour, int nowMinute, int wakeupHour, int wakeupMinute) {
  if (dayIndex >= 7) {
    return false;
  }
  int bedtimeMinutes = config.schedules[dayIndex].hour * 60 + config.schedules[dayIndex].minute;
  int wakeupMinutes = wakeupHour * 60 + wakeupMinute;
  int currentMinutes = nowHour * 60 + nowMinute;
  // Cas typique : coucher le soir (ex. 20:30), lever le matin (ex. 07:00) → bedtimeMinutes > wakeupMinutes
  // Nuit = [bedtime, 24h[ U [0, wakeup[
  if (bedtimeMinutes > wakeupMinutes) {
    return (currentMinutes >= bedtimeMinutes) || (currentMinutes < wakeupMinutes);
  }
  // Cas rare : coucher et lever dans la même "journée" (ex. 01:00 -> 07:00)
  return (currentMinutes >= bedtimeMinutes) && (currentMinutes < wakeupMinutes);
}

// Fenêtre wakeup = 15 min avant lever jusqu'à 35 min après (fade-in + durée + fade-out)
static const int WAKEUP_WINDOW_MINUTES_BEFORE = 1;
static const int WAKEUP_WINDOW_MINUTES_AFTER = 35;

bool BedtimeManager::isCurrentTimeInWakeupWindow(int nowHour, int nowMinute, int wakeupHour, int wakeupMinute) {
  int wakeupMinutes = wakeupHour * 60 + wakeupMinute;
  int currentMinutes = nowHour * 60 + nowMinute;
  int wStart = wakeupMinutes - WAKEUP_WINDOW_MINUTES_BEFORE;
  int wEnd = wakeupMinutes + WAKEUP_WINDOW_MINUTES_AFTER;
  if (wStart < 0) {
    wStart += 24 * 60;
  }
  if (wEnd > 24 * 60) {
    wEnd -= 24 * 60;
  }
  // Fenêtre ne croisant pas minuit : [wStart, wEnd[
  if (wStart < wEnd) {
    return (currentMinutes >= wStart && currentMinutes < wEnd);
  }
  // Fenêtre à cheval sur minuit (ex: lever 00:10 → [0, 45) ou [1425, 1440))
  return (currentMinutes >= wStart) || (currentMinutes < wEnd);
}

const char* BedtimeManager::indexToWeekday(uint8_t index) {
  const char* weekdays[] = {"monday", "tuesday", "wednesday", "thursday", "friday", "saturday", "sunday"};
  if (index < 7) {
    return weekdays[index];
  }
  return weekdays[0];
}

void BedtimeManager::update() {
  if (!initialized) {
    return;
  }
  
  if (!RTCManager::isAvailable()) {
    // Log seulement toutes les 5 minutes pour éviter le spam
    static unsigned long lastRtcErrorLog = 0;
    unsigned long currentTime = millis();
    if (currentTime - lastRtcErrorLog >= 300000) { // 5 minutes
      Serial.println("[BEDTIME] ERREUR: RTC non disponible, impossible de vérifier l'heure");
      lastRtcErrorLog = currentTime;
    }
    return;
  }
  
  DateTime now = RTCManager::getDateTime();
  
  // Vérifier si le jour a changé
  if (lastCheckedDay != now.dayOfWeek) {
    Serial.printf("[BEDTIME] Changement de jour detecte: %d -> %d\n", lastCheckedDay, now.dayOfWeek);
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
    // Si on n'est pas en bedtime mais qu'on est déjà dans la plage coucher->lever (ex: RTC sync après init), activer bedtime sauf si dans fenêtre wakeup
    if (!bedtimeActive && !manuallyStarted && checkingEnabled) {
      DateTime now = RTCManager::getDateTime();
      uint8_t dayIndex = weekdayToIndex(now.dayOfWeek);
      int wakeupHour = 7, wakeupMinute = 0;
      if (getWakeupScheduleForDay(dayIndex, wakeupHour, wakeupMinute) &&
          isCurrentTimeBetweenBedtimeAndWakeup(dayIndex, now.hour, now.minute, wakeupHour, wakeupMinute) &&
          !isCurrentTimeInWakeupWindow(now.hour, now.minute, wakeupHour, wakeupMinute)) {
        Serial.println("[BEDTIME] Heure dans la plage coucher->lever (rattrapage), activation de la routine bedtime");
        startBedtime();
        fadeInActive = false;
        uint8_t brightnessValue = (config.brightness * 255 + 50) / 100;
        LEDManager::setBrightness(brightnessValue);
        lastTriggeredHour = config.schedules[dayIndex].hour;
        lastTriggeredMinute = config.schedules[dayIndex].minute;
      } else {
        checkBedtimeTrigger();
      }
    } else {
      checkBedtimeTrigger();
    }
  }
  
  // Mettre à jour les animations de fade si actives
  if (fadeInActive) {
    updateFadeIn();
  }
  
  // Vérifier si on doit démarrer le fade-out (30 minutes après le début si allNight = false)
  if (bedtimeActive && !config.allNight && !fadeInActive && !fadeOutActive) {
    unsigned long elapsedSinceStart;
    
    // Gérer le wrap-around de millis() (se produit après ~49 jours)
    if (currentTime >= bedtimeStartTime) {
      elapsedSinceStart = currentTime - bedtimeStartTime;
    } else {
      // Wrap-around détecté
      elapsedSinceStart = (ULONG_MAX - bedtimeStartTime) + currentTime;
    }
    
    if (elapsedSinceStart >= BEDTIME_DURATION_MS) {
      // Démarrer le fade-out après 30 minutes
      fadeOutActive = true;
      fadeStartTime = currentTime;
      Serial.println("[BEDTIME] 30 minutes écoulées, démarrage du fade-out (5 minutes de fade-out)");
    }
  }
  
  if (fadeOutActive) {
    updateFadeOut();
  }
}

void BedtimeManager::updateCheckingState() {
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
    Serial.printf("[BEDTIME] Routine activee pour aujourd'hui (%s), verification adaptative activee (intervalle: %lu ms = %.1f heures)\n",
                  indexToWeekday(dayIndex), interval, interval / 3600000.0f);
  } else {
    Serial.printf("[BEDTIME] Routine non activee pour aujourd'hui (%s), verification desactivee jusqu'au jour suivant\n",
                  indexToWeekday(dayIndex));
  }
}

bool BedtimeManager::configChanged() {
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
      config.brightness != lastConfig.brightness ||
      config.allNight != lastConfig.allNight ||
      strcmp(config.effect, lastConfig.effect) != 0) {
    return true;
  }
  
  return false;
}

unsigned long BedtimeManager::calculateNextCheckInterval() {
  if (!RTCManager::isAvailable()) {
    return CHECK_INTERVAL_MS;  // Par défaut, toutes les minutes
  }
  
  DateTime now = RTCManager::getDateTime();
  uint8_t dayIndex = weekdayToIndex(now.dayOfWeek);
  
  if (!config.schedules[dayIndex].activated) {
    return CHECK_INTERVAL_3H_MS;  // Non activé, vérifier toutes les 3h au cas où
  }
  
  // Calculer la distance jusqu'à l'heure de déclenchement
  int targetHour = config.schedules[dayIndex].hour;
  int targetMinute = config.schedules[dayIndex].minute;
  
  // Calculer les minutes jusqu'à l'heure de déclenchement
  int currentMinutes = now.hour * 60 + now.minute;
  int targetMinutes = targetHour * 60 + targetMinute;
  int minutesUntilTarget = targetMinutes - currentMinutes;
  
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

void BedtimeManager::checkBedtimeTrigger() {
  DateTime now = RTCManager::getDateTime();
  uint8_t dayIndex = weekdayToIndex(now.dayOfWeek);
  
  // Log de débogage pour chaque vérification
  Serial.printf("[BEDTIME] Vérification: Heure actuelle %02d:%02d:%02d, Jour de la semaine: %d (index: %d)\n",
                now.hour, now.minute, now.second, now.dayOfWeek, dayIndex);
  Serial.printf("[BEDTIME] Configuration pour ce jour: %02d:%02d, Activé: %s\n",
                config.schedules[dayIndex].hour, config.schedules[dayIndex].minute,
                config.schedules[dayIndex].activated ? "Oui" : "Non");
  
  // Vérifier si le bedtime est activé pour aujourd'hui
  if (!config.schedules[dayIndex].activated) {
    Serial.println("[BEDTIME] Le bedtime n'est pas activé pour aujourd'hui");
    // Si le bedtime était actif mais le jour n'est plus activé, l'arrêter
    if (bedtimeActive) {
      Serial.println("[BEDTIME] Arrêt du bedtime car le jour n'est plus activé");
      stopBedtime();
    }
    return;
  }
  
  int targetHour = config.schedules[dayIndex].hour;
  int targetMinute = config.schedules[dayIndex].minute;
  int currentMinutes = now.hour * 60 + now.minute;
  int targetMinutes = targetHour * 60 + targetMinute;

  // Vérifier si c'est l'heure de coucher exacte (dans la minute, secondes 0-59)
  // On vérifie toutes les minutes, donc on déclenche si on est dans la bonne minute
  if (now.hour == targetHour && now.minute == targetMinute) {
    Serial.printf("[BEDTIME] Heure correspondante détectée! Bedtime actif: %s, Last triggered: %02d:%02d\n",
                  bedtimeActive ? "Oui" : "Non", lastTriggeredHour, lastTriggeredMinute);

    // Déclencher le bedtime si pas déjà actif et qu'on n'a pas déjà déclenché cette minute
    // ET que le bedtime n'a pas été démarré manuellement
    if (!bedtimeActive &&
        !manuallyStarted &&
        (lastTriggeredHour != now.hour || lastTriggeredMinute != now.minute)) {
      Serial.println("[BEDTIME] >>> DÉCLENCHEMENT DU BEDTIME <<<");
      startBedtime();
      lastTriggeredHour = now.hour;
      lastTriggeredMinute = now.minute;
    } else {
      if (bedtimeActive) {
        Serial.println("[BEDTIME] Bedtime déjà actif, pas de nouveau déclenchement");
      } else if (manuallyStarted) {
        Serial.println("[BEDTIME] Bedtime démarré manuellement, pas de déclenchement automatique");
      } else {
        Serial.println("[BEDTIME] Déjà déclenché cette minute, pas de nouveau déclenchement");
      }
    }
  } else {
    // Sécurité : si on a dépassé l'heure de coucher de 0 à 2 minutes et qu'on n'est pas en mode bedtime, déclencher
    int minutesAfterTarget = currentMinutes - targetMinutes;
    if (minutesAfterTarget >= 0 && minutesAfterTarget <= 2 &&
        !bedtimeActive && !manuallyStarted &&
        (lastTriggeredHour != (uint8_t)targetHour || lastTriggeredMinute != (uint8_t)targetMinute)) {
      Serial.println("[BEDTIME] >>> DÉCLENCHEMENT SÉCURITÉ (dépassement 0-2 min) <<<");
      startBedtime();
      lastTriggeredHour = targetHour;
      lastTriggeredMinute = targetMinute;
    } else {
      // Log pour comprendre pourquoi ça ne correspond pas
      Serial.printf("[BEDTIME] Heure ne correspond pas: Actuelle %02d:%02d vs Config %02d:%02d\n",
                    now.hour, now.minute, targetHour, targetMinute);

      // Si on n'est plus dans la minute de déclenchement, réinitialiser les flags
      if (lastTriggeredHour == config.schedules[dayIndex].hour &&
          lastTriggeredMinute == config.schedules[dayIndex].minute) {
        Serial.println("[BEDTIME] Sortie de la minute de déclenchement, réinitialisation des flags");
        lastTriggeredHour = 255;
        lastTriggeredMinute = 255;
      }
    }
  }
}

void BedtimeManager::startBedtime() {
  Serial.println("[BEDTIME] Démarrage du bedtime automatique");
  
  bedtimeActive = true;
  bedtimeStartTime = millis();
  fadeInActive = true;
  fadeOutActive = false;
  fadeStartTime = millis();
  
  // Convertir brightness de 0-100 vers 0-255
  uint8_t brightnessValue = (config.brightness * 255 + 50) / 100;
  
  // Empêcher le sleep mode pendant le bedtime
  LEDManager::preventSleep();
  
  // Réveiller les LEDs
  LEDManager::wakeUp();
  
  // Déterminer l'effet à utiliser
  LEDEffect effect = LED_EFFECT_NONE;
  bool useEffect = false;
  
  // Vérifier si un effet est configuré (et n'est pas "none")
  if (strlen(config.effect) > 0 && strcmp(config.effect, "none") != 0) {
    useEffect = true;
    
    // Convertir le nom de l'effet en enum
    if (strcmp(config.effect, "pulse") == 0) {
      effect = LED_EFFECT_PULSE;
    } else if (strcmp(config.effect, "rainbow-soft") == 0) {
      effect = LED_EFFECT_RAINBOW_SOFT;
    } else if (strcmp(config.effect, "breathe") == 0) {
      effect = LED_EFFECT_BREATHE;
    } else if (strcmp(config.effect, "nightlight") == 0) {
      effect = LED_EFFECT_NIGHTLIGHT;
    } else {
      // Effet inconnu, utiliser couleur fixe
      useEffect = false;
      effect = LED_EFFECT_NONE;
      Serial.printf("[BEDTIME] Effet inconnu: %s, utilisation de la couleur fixe\n", config.effect);
    }
  }
  
  if (useEffect) {
    // Mode effet animé
    LEDManager::setEffect(effect);
    // Définir aussi la couleur pour les effets qui l'utilisent (comme ROTATE)
    LEDManager::setColor(config.colorR, config.colorG, config.colorB);
    Serial.printf("[BEDTIME] Effet: %s, Couleur RGB(%d, %d, %d), Brightness cible: %d%%\n",
                  config.effect, config.colorR, config.colorG, config.colorB, config.brightness);
  } else {
    // Mode couleur fixe
    LEDManager::setEffect(LED_EFFECT_NONE);
    LEDManager::setColor(config.colorR, config.colorG, config.colorB);
    Serial.printf("[BEDTIME] Couleur RGB(%d, %d, %d), Brightness cible: %d%%\n",
                  config.colorR, config.colorG, config.colorB, config.brightness);
  }
}

void BedtimeManager::updateFadeIn() {
  unsigned long elapsed = millis() - fadeStartTime;
  
  if (elapsed >= FADE_IN_DURATION_MS) {
    // Fade-in terminé
    fadeInActive = false;
    
    // Appliquer la brightness finale
    uint8_t brightnessValue = (config.brightness * 255 + 50) / 100;
    LEDManager::setBrightness(brightnessValue);
    
    Serial.println("[BEDTIME] Fade-in termine");
  } else {
    // Interpolation linéaire de la brightness
    float progress = (float)elapsed / (float)FADE_IN_DURATION_MS;
    uint8_t targetBrightness = (config.brightness * 255 + 50) / 100;
    uint8_t currentBrightness = (uint8_t)(progress * targetBrightness);
    
    LEDManager::setBrightness(currentBrightness);
  }
}

void BedtimeManager::updateFadeOut() {
  unsigned long elapsed = millis() - fadeStartTime;
  
  if (elapsed >= FADE_OUT_DURATION_MS) {
    // Fade-out terminé, éteindre complètement et arrêter le bedtime
    fadeOutActive = false;
    LEDManager::clear();
    bedtimeActive = false; // Arrêter le bedtime après le fade-out
    manuallyStarted = false; // Réinitialiser le flag manuel
    Serial.println("[BEDTIME] Fade-out termine, LEDs eteintes, bedtime arrete");
  } else {
    // Interpolation linéaire de la brightness vers 0
    float progress = (float)elapsed / (float)FADE_OUT_DURATION_MS;
    uint8_t startBrightness = (config.brightness * 255 + 50) / 100;
    uint8_t currentBrightness = (uint8_t)(startBrightness * (1.0f - progress));
    
    LEDManager::setBrightness(currentBrightness);
  }
}

void BedtimeManager::stopBedtime(bool clearDisplay) {
  Serial.println("[BEDTIME] Arrêt du bedtime");
  
  bedtimeActive = false;
  fadeInActive = false;
  fadeOutActive = false;
  manuallyStarted = false; // Réinitialiser le flag manuel

  // Réautoriser le sleep mode
  LEDManager::allowSleep();
  
  // Éteindre les LEDs sauf si transition vers wakeup (on garde l'affichage pour le fade progressif)
  if (clearDisplay) {
    LEDManager::clear();
  }
}

bool BedtimeManager::isBedtimeEnabled() {
  if (!initialized) {
    return false;
  }
  
  DateTime now = RTCManager::getDateTime();
  uint8_t dayIndex = weekdayToIndex(now.dayOfWeek);
  
  return config.schedules[dayIndex].activated;
}

BedtimeConfig BedtimeManager::getConfig() {
  return config;
}

bool BedtimeManager::isBedtimeActive() {
  return bedtimeActive;
}

void BedtimeManager::startBedtimeManually() {
  Serial.println("[BEDTIME] Démarrage manuel du bedtime (force remise en route selon la config)");
  
  // Si le bedtime est déjà marqué actif (même si les LEDs ne le reflètent pas), arrêter proprement
  // pour repartir sur une base saine et réappliquer la config
  if (bedtimeActive || fadeInActive || fadeOutActive) {
    stopBedtime();
  }
  
  // Marquer comme démarré manuellement pour empêcher le déclenchement automatique
  manuallyStarted = true;
  
  // Démarrer le bedtime selon la config (LEDs, effet, fade-in, etc.)
  startBedtime();
}

void BedtimeManager::stopBedtimeManually() {
  Serial.println("[BEDTIME] Arrêt manuel du bedtime");
  
  // Arrêter le bedtime (qui réinitialisera aussi manuallyStarted)
  stopBedtime();
}

void BedtimeManager::restoreDisplayFromConfig() {
  if (!initialized) {
    return;
  }
  // Ne pas écraser si on est en extinction progressive (allNight = false, fade-out en cours)
  if (fadeOutActive) {
    return;
  }
  // Réafficher effet, couleur et luminosité selon la config (sans toucher à bedtimeActive/fade)
  LEDManager::preventSleep();
  LEDManager::wakeUp();

  uint8_t brightnessValue = (config.brightness * 255 + 50) / 100;
  LEDEffect effect = LED_EFFECT_NONE;
  bool useEffect = false;

  if (strlen(config.effect) > 0 && strcmp(config.effect, "none") != 0) {
    useEffect = true;
    if (strcmp(config.effect, "pulse") == 0) {
      effect = LED_EFFECT_PULSE;
    } else if (strcmp(config.effect, "rainbow-soft") == 0) {
      effect = LED_EFFECT_RAINBOW_SOFT;
    } else if (strcmp(config.effect, "breathe") == 0) {
      effect = LED_EFFECT_BREATHE;
    } else if (strcmp(config.effect, "nightlight") == 0) {
      effect = LED_EFFECT_NIGHTLIGHT;
    } else {
      useEffect = false;
    }
  }

  if (useEffect) {
    LEDManager::setEffect(effect);
    LEDManager::setColor(config.colorR, config.colorG, config.colorB);
  } else {
    LEDManager::setEffect(LED_EFFECT_NONE);
    LEDManager::setColor(config.colorR, config.colorG, config.colorB);
  }
  LEDManager::setBrightness(brightnessValue);
  Serial.println("[BEDTIME] Affichage restaure selon la config (retour mode bedtime)");
}
