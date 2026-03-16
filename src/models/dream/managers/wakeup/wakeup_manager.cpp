#include "wakeup_manager.h"
#include "../dream_schedules.h"
#include "../dream_timing_constants.h"
#include "../dream_rtc_macros.h"
#include "../../../../common/utils/time_utils.h"
#include "../touch/dream_touch_handler.h"
#include "../../pubnub/model_pubnub_routes.h"
#include "../../utils/schedule_parser.h"
#include "../schedule_utils.h"
#include <ArduinoJson.h>
#include "../bedtime/bedtime_manager.h"

// Variables statiques
ScheduleState WakeupManager::s_state;
WakeupConfig WakeupManager::config;
WakeupConfig WakeupManager::lastConfig;
ColorState WakeupManager::s_color;
uint8_t WakeupManager::startBrightness = 0;
uint8_t WakeupManager::lastBrightness = 255;

// Constantes
// Les durées FADE_IN/FADE_OUT/FADE_UPDATE sont dans DreamTiming (dream_timing_constants.h)
static const int WAKEUP_TRIGGER_MINUTES_BEFORE = 5;          // Déclencher 5 minutes avant

bool WakeupManager::init() {
  if (s_state.initialized) {
    return true;
  }
  
  RTC_CHECK_OR_RETURN_FALSE("WAKEUP");

  // Charger la configuration depuis la SD
  if (!loadConfig()) {
    Serial.println("[WAKEUP] ERREUR: Impossible de charger la configuration");
    return false;
  }
  
  s_state.initialized = true;
  
  // Initialiser l'état de vérification
  if (RTCManager::isAvailable()) {
    DateTime now = RTCManager::getLocalDateTime();
    s_state.lastCheckedDay = now.dayOfWeek;
    updateCheckingState();
    
    // Initialiser lastCheckTime pour démarrer avec le bon intervalle
    s_state.lastCheckTime = millis();
    
#ifdef DREAM_DEBUG
    if (s_state.checkingEnabled) {
      unsigned long interval = calculateNextCheckInterval();
      Serial.printf("[WAKEUP] Intervalle de verification initial: %lu ms (%.1f heures)\n",
                    interval, interval / 3600000.0f);
    }
#endif

    // Au démarrage : si l'heure actuelle est dans la fenêtre wakeup (15 min avant lever → 35 min après),
    // démarrer la routine wakeup (bedtime ne l'a pas fait pour ne pas écraser ce mode)
    if (s_state.checkingEnabled) {
      uint8_t dayIndex = ScheduleUtils::weekdayToIndex(now.dayOfWeek);
      int wakeupHour = config.schedules[dayIndex].hour;
      int wakeupMinute = config.schedules[dayIndex].minute;
      int wakeupMinutes = TimeUtils::timeToMinutes(wakeupHour, wakeupMinute);
      int currentMinutes = TimeUtils::timeToMinutes(now.hour, now.minute);
      int wStart = wakeupMinutes - WAKEUP_TRIGGER_MINUTES_BEFORE;
      int wEnd = wakeupMinutes + 35;  // 35 min après = fade-out terminé
      if (wStart < 0) {
        wStart += DreamTiming::MINUTES_PER_DAY;
      }
      if (wEnd > DreamTiming::MINUTES_PER_DAY) {
        wEnd -= DreamTiming::MINUTES_PER_DAY;
      }
      bool inWakeupWindow = (wStart < wEnd)
        ? (currentMinutes >= wStart && currentMinutes < wEnd)
        : ((currentMinutes >= wStart) || (currentMinutes < wEnd));
      if (inWakeupWindow) {
        startWakeup();
        s_state.lastTriggeredHour = now.hour;
        s_state.lastTriggeredMinute = now.minute;
      }
    }
  } else {
    s_state.checkingEnabled = false;
    s_state.lastCheckedDay = 0;
    s_state.lastCheckTime = millis();
  }
  
#ifdef DREAM_DEBUG
  Serial.println("[WAKEUP] Gestionnaire initialise");
#endif
  
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
  config.autoShutdown = sdConfig.wakeup_autoShutdown;
  config.autoShutdownMinutes = sdConfig.wakeup_autoShutdownMinutes;
  
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
  
  return true;
}

void WakeupManager::loadBedtimeColor() {
  // Couleur de départ pour le fade-in vers la couleur de réveil.
  // Si le bedtime est actif : toujours prendre la couleur actuellement affichée (effet ou fixe)
  // pour que la transition soit progressive depuis l'état réel des LEDs.
  BedtimeConfig bedtimeConfig = BedtimeManager::getConfig();

  if (BedtimeManager::isBedtimeActive()) {
    // Bedtime en cours : récupérer la couleur réellement affichée (effet ou couleur fixe)
    LEDManager::getCurrentColor(s_color.startR, s_color.startG, s_color.startB);
#ifdef DREAM_DEBUG
    Serial.printf("[WAKEUP] Transition depuis bedtime -> couleur actuelle LEDs: RGB(%d, %d, %d)\n",
                  s_color.startR, s_color.startG, s_color.startB);
#endif
  } else {
    // Bedtime inactif (ex: boot dans fenêtre wakeup) : utiliser la couleur de la config
    s_color.startR = bedtimeConfig.colorR;
    s_color.startG = bedtimeConfig.colorG;
    s_color.startB = bedtimeConfig.colorB;
  }
}

bool WakeupManager::reloadConfig() {
  Serial.println("[WAKEUP] >>> RELOAD CONFIG <<<");

  // Réinitialiser les flags de déclenchement SEULEMENT si pas actif
  // (ne pas interrompre une transition en cours)
  if (!s_state.routineActive) {
    ScheduleUtils::resetTriggeredFlags(s_state);
  }

  bool result = loadConfig();
  Serial.printf("[WAKEUP] loadConfig() result: %s\n", result ? "true" : "false");

  // Si la config a changé, vérifier si la routine est activée pour aujourd'hui
  if (result && s_state.initialized && RTCManager::isAvailable()) {
    DateTime now = RTCManager::getLocalDateTime();
    uint8_t dayIndex = ScheduleUtils::weekdayToIndex(now.dayOfWeek);

    bool changed = configChanged();
    Serial.printf("[WAKEUP] Config changed: %s\n", changed ? "true" : "false");

    if (changed) {
      Serial.printf("[WAKEUP] Nouvelle config: %02d:%02d (Jour: %d, Index: %d)\n",
                    config.schedules[dayIndex].hour,
                    config.schedules[dayIndex].minute,
                    now.dayOfWeek, dayIndex);
      Serial.println("[WAKEUP] Configuration modifiee, verification de l'etat pour aujourd'hui");
      updateCheckingState();
      Serial.printf("[WAKEUP] Checking enabled: %s\n", s_state.checkingEnabled ? "true" : "false");

      // Réinitialiser lastCheckTime pour recalculer l'intervalle avec la nouvelle config
      s_state.lastCheckTime = millis();

      // Si maintenant activé pour aujourd'hui, vérifier immédiatement
      if (s_state.checkingEnabled) {
        Serial.println("[WAKEUP] Routine activée pour aujourd'hui - appel checkNow()");
        checkNow();
      } else {
        Serial.println("[WAKEUP] Routine non activée pour aujourd'hui");
      }
    } else {
      Serial.println("[WAKEUP] Config identique");
      // Config identique, juste vérifier maintenant si déjà en cours de vérification
      if (s_state.checkingEnabled) {
        Serial.println("[WAKEUP] Checking déjà activé - appel checkNow()");
        // Réinitialiser quand même lastCheckTime pour recalculer l'intervalle (au cas où l'heure aurait changé)
        s_state.lastCheckTime = millis();
        checkNow();
      } else {
        Serial.println("[WAKEUP] Checking non activé");
      }
    }
  } else {
    Serial.printf("[WAKEUP] Conditions non remplies pour vérifier: result=%s, initialized=%s, RTC available=%s\n",
                  result ? "true" : "false",
                  s_state.initialized ? "true" : "false",
                  RTCManager::isAvailable() ? "true" : "false");
  }

  return result;
}

void WakeupManager::checkNow() {
  if (!s_state.initialized || !RTCManager::isAvailable()) {
    return;
  }
  
  // Vérifier immédiatement si c'est l'heure de déclencher le wake-up
  checkWakeupTrigger();
}

void WakeupManager::parseWeekdaySchedule(const char* jsonStr) {
  // Utiliser ScheduleParser pour parser le JSON weekdaySchedule
  // validateFully=false pour wakeup (moins strict que bedtime)
  ScheduleParser::parseWeekdaySchedule(jsonStr, config.schedules, false, "[WAKEUP]");
}

void WakeupManager::update() {
  if (!s_state.initialized) {
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
  
  DateTime now = RTCManager::getLocalDateTime();
  
  // Vérifier si le jour a changé
  if (s_state.lastCheckedDay != now.dayOfWeek) {
    s_state.lastCheckedDay = now.dayOfWeek;
    updateCheckingState();  // Mettre à jour l'état de vérification pour le nouveau jour
  }
  
  // Ne vérifier que si la routine est activée pour aujourd'hui
  if (!s_state.checkingEnabled) {
    return;  // Routine non activée pour aujourd'hui, pas besoin de vérifier
  }
  
  unsigned long currentTime = millis();
  
  // Calculer le prochain intervalle de vérification basé sur la distance jusqu'à l'heure de déclenchement
  unsigned long nextCheckInterval = calculateNextCheckInterval();
  
  // Vérifier périodiquement avec un intervalle adaptatif
  unsigned long elapsed = TimeUtils::calculateElapsed(currentTime, s_state.lastCheckTime);
  
  if (elapsed >= nextCheckInterval) {
    s_state.lastCheckTime = currentTime;
    checkWakeupTrigger();
  }
  
  // Ne pas écraser le feedback alerte (vert/rouge pulsé)
#ifdef HAS_TOUCH
  if (DreamTouchHandler::s_alertFeedbackUntil > 0 && currentTime < DreamTouchHandler::s_alertFeedbackUntil) {
    return;
  }
#endif

  // Mettre à jour les animations de fade si actives (avec throttling pour éviter les appels trop fréquents)
  if (s_state.fadeInActive) {
    unsigned long timeSinceLastFadeUpdate = TimeUtils::calculateElapsed(currentTime, s_state.lastFadeUpdateTime);

    if (timeSinceLastFadeUpdate >= DreamTiming::FADE_UPDATE_INTERVAL_MS) {
      s_state.lastFadeUpdateTime = currentTime;
      updateFadeIn();
    }
  }
  
  // Vérifier si on doit démarrer le fade-out (seulement si l'extinction automatique est activée)
  if (config.autoShutdown && s_state.routineActive && !s_state.fadeInActive && !s_state.fadeOutActive) {
    // Calculer le temps écoulé depuis le début du wake-up
    unsigned long elapsedSinceStart;

    // Gérer le wrap-around de millis() (se produit après ~49 jours)
    if (currentTime >= s_state.startTime) {
      elapsedSinceStart = currentTime - s_state.startTime;
    } else {
      // Wrap-around détecté
      elapsedSinceStart = (ULONG_MAX - s_state.startTime) + currentTime;
    }

    // Calculer le temps avant fade-out : fade-in + durée d'extinction
    unsigned long durationMs = (unsigned long)config.autoShutdownMinutes * 60000UL;
    unsigned long fadeOutStartMs = DreamTiming::FADE_IN_DURATION_MS + durationMs;

    // Démarrer le fade-out après la durée d'extinction configurée
    if (elapsedSinceStart >= fadeOutStartMs) {
      s_state.fadeOutActive = true;
      s_state.fadeStartTime = currentTime;
      Serial.printf("[WAKEUP] %u minutes écoulées, démarrage du fade-out (5 minutes de fade-out)\n", config.autoShutdownMinutes);
    }
  }
  
  // Mettre à jour le fade-out si actif (avec throttling)
  if (s_state.fadeOutActive) {
    unsigned long timeSinceLastFadeUpdate = TimeUtils::calculateElapsed(currentTime, s_state.lastFadeUpdateTime);

    if (timeSinceLastFadeUpdate >= DreamTiming::FADE_UPDATE_INTERVAL_MS) {
      s_state.lastFadeUpdateTime = currentTime;
      updateFadeOut();
    }
  }
}

void WakeupManager::updateCheckingState() {
  if (!RTCManager::isAvailable()) {
    s_state.checkingEnabled = false;
    return;
  }
  
  DateTime now = RTCManager::getLocalDateTime();
  uint8_t dayIndex = ScheduleUtils::weekdayToIndex(now.dayOfWeek);
  
  // Vérifier si la routine est activée pour aujourd'hui
  bool wasEnabled = s_state.checkingEnabled;
  s_state.checkingEnabled = config.schedules[dayIndex].activated;
  
  if (s_state.checkingEnabled) {
    if (!wasEnabled) {
      // Réinitialiser lastCheckTime quand on active la vérification
      s_state.lastCheckTime = millis();
    }
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
  
  DateTime now = RTCManager::getLocalDateTime();
  uint8_t dayIndex = ScheduleUtils::weekdayToIndex(now.dayOfWeek);
  
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
  int currentMinutes = TimeUtils::timeToMinutes(now.hour, now.minute);
  int triggerMinutes = TimeUtils::timeToMinutes(targetHour, targetMinute);
  int minutesUntilTarget = triggerMinutes - currentMinutes;
  
  // Si l'heure de déclenchement est passée aujourd'hui, c'est pour demain
  if (minutesUntilTarget < 0) {
    minutesUntilTarget += DreamTiming::MINUTES_PER_DAY;
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
  DateTime now = RTCManager::getLocalDateTime();
  uint8_t dayIndex = ScheduleUtils::weekdayToIndex(now.dayOfWeek);
  
#ifdef DREAM_DEBUG
  Serial.printf("[WAKEUP] Vérification: Heure actuelle %02d:%02d:%02d, Jour: %d (index: %d)\n",
                now.hour, now.minute, now.second, now.dayOfWeek, dayIndex);
  Serial.printf("[WAKEUP] Configuration: %02d:%02d, Activé: %s\n",
                config.schedules[dayIndex].hour, config.schedules[dayIndex].minute,
                config.schedules[dayIndex].activated ? "Oui" : "Non");
#endif
  
  if (!config.schedules[dayIndex].activated) {
#ifdef DREAM_DEBUG
    Serial.println("[WAKEUP] Le wake-up n'est pas activé pour aujourd'hui");
#endif
    if (s_state.routineActive) {
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
    // Déclencher le wake-up si pas déjà actif et qu'on n'a pas déjà déclenché cette minute
    if (!s_state.routineActive && 
        (s_state.lastTriggeredHour != now.hour || s_state.lastTriggeredMinute != now.minute)) {
      startWakeup();
      s_state.lastTriggeredHour = now.hour;
      s_state.lastTriggeredMinute = now.minute;
    } else {
#ifdef DREAM_DEBUG
      if (s_state.routineActive) {
        Serial.println("[WAKEUP] Wake-up déjà actif, pas de nouveau déclenchement");
      } else {
        Serial.println("[WAKEUP] Déjà déclenché cette minute, pas de nouveau déclenchement");
      }
#endif
    }
  } else {
#ifdef DREAM_DEBUG
    Serial.printf("[WAKEUP] Heure ne correspond pas: Actuelle %02d:%02d vs Trigger %02d:%02d (réveil à %02d:%02d)\n",
                  now.hour, now.minute, triggerHour, triggerMinute,
                  config.schedules[dayIndex].hour, config.schedules[dayIndex].minute);
#endif
    if (s_state.lastTriggeredHour == triggerHour && 
        s_state.lastTriggeredMinute == triggerMinute) {
#ifdef DREAM_DEBUG
      Serial.println("[WAKEUP] Sortie de la minute de déclenchement, réinitialisation des flags");
#endif
      ScheduleUtils::resetTriggeredFlags(s_state);
    }
  }
}

void WakeupManager::startWakeup() {
  ModelDreamPubNubRoutes::publishRoutineState("wakeup", "started");

  s_state.routineActive = true;
  s_state.startTime = millis();
  s_state.fadeInActive = true;
  s_state.fadeOutActive = false;
  s_state.fadeStartTime = millis();
  
  // 1) Capturer l'état actuel des LEDs (couleur + brightness) AVANT tout changement
  //    pour transition progressive depuis l'effet/couleur du bedtime vers la couleur de réveil
  loadBedtimeColor();
  startBrightness = LEDManager::getCurrentBrightness();
  
  // 2) Arrêter le bedtime sans éteindre les LEDs (évite un flash et garde l'affichage pour le fade)
  if (BedtimeManager::isBedtimeActive()) {
    BedtimeManager::stopBedtime(false);
  }
  
  // Empêcher le sleep mode pendant le wake-up
  LEDManager::preventSleep();

  // Réveiller les LEDs
  LEDManager::wakeUp();

  // 3) Figer l'affichage sur la couleur/brightness capturées puis lancer le fade progressif
  if (!LEDManager::setEffect(LED_EFFECT_NONE)) {
    Serial.println("[WAKEUP] WARN: setEffect(NONE) failed");
  }
  if (!LEDManager::setColor(s_color.startR, s_color.startG, s_color.startB)) {
    Serial.printf("[WAKEUP] WARN: setColor(%d,%d,%d) failed\n", s_color.startR, s_color.startG, s_color.startB);
  }
  if (!LEDManager::setBrightness(startBrightness)) {
    Serial.printf("[WAKEUP] WARN: setBrightness(%d) failed\n", startBrightness);
  }
  
  // Initialiser les dernières valeurs pour le fade-in
  s_color.lastR = s_color.startR;
  s_color.lastG = s_color.startG;
  s_color.lastB = s_color.startB;
  lastBrightness = startBrightness;
}

void WakeupManager::updateFadeIn() {
  unsigned long elapsed;
  unsigned long currentTime = millis();
  
  // Gérer le wrap-around de millis()
  if (currentTime >= s_state.fadeStartTime) {
    elapsed = currentTime - s_state.fadeStartTime;
  } else {
    elapsed = (ULONG_MAX - s_state.fadeStartTime) + currentTime;
  }
  
  if (elapsed >= DreamTiming::FADE_IN_DURATION_MS) {
    // Fade-in terminé
    s_state.fadeInActive = false;
    
    // Appliquer la couleur et brightness finales seulement si elles ont changé
    if (s_color.lastR != config.colorR || s_color.lastG != config.colorG || s_color.lastB != config.colorB) {
      if (!LEDManager::setColor(config.colorR, config.colorG, config.colorB)) {
        Serial.printf("[WAKEUP] WARN: setColor(%d,%d,%d) failed\n", config.colorR, config.colorG, config.colorB);
      } else {
        s_color.lastR = config.colorR;
        s_color.lastG = config.colorG;
        s_color.lastB = config.colorB;
      }
    }

    uint8_t brightnessValue = LEDManager::brightnessPercentTo255(config.brightness);
    if (lastBrightness != brightnessValue) {
      if (!LEDManager::setBrightness(brightnessValue)) {
        Serial.printf("[WAKEUP] WARN: setBrightness(%d) failed\n", brightnessValue);
      } else {
        lastBrightness = brightnessValue;
      }
    }
  } else {
    // Interpolation linéaire de la brightness et de la couleur (utiliser entiers au lieu de float)

    // Brightness: startBrightness → targetBrightness (ne pas repartir de 0)
    uint8_t targetBrightness = LEDManager::brightnessPercentTo255(config.brightness);
    uint8_t currentBrightness = (uint8_t)(startBrightness + ((targetBrightness - startBrightness) * elapsed) / DreamTiming::FADE_IN_DURATION_MS);

    // Couleur: interpolation linéaire RGB de startColor vers config.color (utiliser entiers)
    uint8_t currentR = (uint8_t)(s_color.startR + ((config.colorR - s_color.startR) * elapsed) / DreamTiming::FADE_IN_DURATION_MS);
    uint8_t currentG = (uint8_t)(s_color.startG + ((config.colorG - s_color.startG) * elapsed) / DreamTiming::FADE_IN_DURATION_MS);
    uint8_t currentB = (uint8_t)(s_color.startB + ((config.colorB - s_color.startB) * elapsed) / DreamTiming::FADE_IN_DURATION_MS);

    // Ne mettre à jour que si la couleur ou brightness a changé
    if (s_color.lastR != currentR || s_color.lastG != currentG || s_color.lastB != currentB) {
      if (!LEDManager::setColor(currentR, currentG, currentB)) {
        Serial.printf("[WAKEUP] WARN: setColor(%d,%d,%d) failed\n", currentR, currentG, currentB);
      } else {
        s_color.lastR = currentR;
        s_color.lastG = currentG;
        s_color.lastB = currentB;
      }
    }

    if (lastBrightness != currentBrightness) {
      if (!LEDManager::setBrightness(currentBrightness)) {
        Serial.printf("[WAKEUP] WARN: setBrightness(%d) failed\n", currentBrightness);
      } else {
        lastBrightness = currentBrightness;
      }
    }
  }
}

void WakeupManager::updateFadeOut() {
  unsigned long elapsed;
  unsigned long currentTime = millis();
  
  // Gérer le wrap-around de millis()
  if (currentTime >= s_state.fadeStartTime) {
    elapsed = currentTime - s_state.fadeStartTime;
  } else {
    elapsed = (ULONG_MAX - s_state.fadeStartTime) + currentTime;
  }
  
  if (elapsed >= DreamTiming::FADE_OUT_DURATION_MS) {
    // Fade-out terminé, éteindre complètement et arrêter le wake-up
    s_state.fadeOutActive = false;
    LEDManager::clear();
    ModelDreamPubNubRoutes::publishRoutineState("wakeup", "stopped");
    s_state.routineActive = false; // Arrêter le wake-up après le fade-out
  } else {
    // Interpolation linéaire de la brightness vers 0
    float progress = (float)elapsed / (float)DreamTiming::FADE_OUT_DURATION_MS;
    uint8_t startBrightness = LEDManager::brightnessPercentTo255(config.brightness);
    uint8_t currentBrightness = (uint8_t)(startBrightness * (1.0f - progress));

    if (!LEDManager::setBrightness(currentBrightness)) {
      Serial.printf("[WAKEUP] WARN: setBrightness(%d) failed\n", currentBrightness);
    }
  }
}

void WakeupManager::stopWakeup() {
  Serial.println("[WAKEUP] Arrêt du wake-up");

  if (s_state.routineActive) {
    ModelDreamPubNubRoutes::publishRoutineState("wakeup", "stopped");
  }

  s_state.routineActive = false;
  s_state.fadeInActive = false;
  s_state.fadeOutActive = false;

  // Réautoriser le sleep mode
  LEDManager::allowSleep();

  // Éteindre les LEDs
  if (!LEDManager::clear()) {
    Serial.println("[WAKEUP] WARN: clear() failed");
  }
}

bool WakeupManager::isWakeupEnabled() {
  if (!s_state.initialized) {
    return false;
  }
  
  DateTime now = RTCManager::getLocalDateTime();
  uint8_t dayIndex = ScheduleUtils::weekdayToIndex(now.dayOfWeek);
  
  return config.schedules[dayIndex].activated;
}

WakeupConfig WakeupManager::getConfig() {
  return config;
}

bool WakeupManager::isWakeupActive() {
  return s_state.routineActive;
}

void WakeupManager::stopWakeupManually() {
  // Arrêter le wake-up et réinitialiser les flags de déclenchement
  // pour empêcher un redéclenchement automatique la même journée
  stopWakeup();
  ScheduleUtils::resetTriggeredFlags(s_state);
}
