#include "bedtime_manager.h"
#include "../dream_schedules.h"
#include "../../../../common/utils/time_utils.h"
#include "../touch/dream_touch_handler.h"
#include "../../pubnub/model_pubnub_routes.h"
#include "../../utils/schedule_parser.h"
#include "../../utils/led_effect_parser.h"
#include "../schedule_utils.h"
#include <ArduinoJson.h>

// Variables statiques
ScheduleState BedtimeManager::s_state;
BedtimeConfig BedtimeManager::config;
BedtimeConfig BedtimeManager::lastConfig;
bool BedtimeManager::manuallyStarted = false;

// Constantes (CHECK_INTERVAL_* partagées dans dream_schedules.h)
static const unsigned long FADE_IN_DURATION_MS = 30000;      // 30 secondes
static const unsigned long FADE_OUT_DURATION_MS = 300000;    // 5 minutes
static const unsigned long BEDTIME_DURATION_MS = 1800000;     // 30 minutes avant fade-out

bool BedtimeManager::init() {
  if (s_state.initialized) {
    return true;
  }
  
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
  
  s_state.initialized = true;
  
  // Initialiser l'état de vérification
  if (RTCManager::isAvailable()) {
    DateTime now = RTCManager::getDateTime();
    s_state.lastCheckedDay = now.dayOfWeek;
    updateCheckingState();
    
    // Initialiser s_state.lastCheckTime pour démarrer avec le bon intervalle
    s_state.lastCheckTime = millis();
    
    // Au démarrage : si on est dans la plage nuit, activer soit bedtime soit laisser WakeupManager démarrer le wakeup
    // (dans la fenêtre 15 min avant lever → 35 min après = mode wakeup, sinon mode bedtime)
    if (s_state.checkingEnabled) {
      uint8_t dayIndex = weekdayToIndex(now.dayOfWeek);
      int wakeupHour = 7, wakeupMinute = 0;
      bool hasWakeup = getWakeupScheduleForDay(dayIndex, wakeupHour, wakeupMinute);
      if (hasWakeup && isCurrentTimeBetweenBedtimeAndWakeup(dayIndex, now.hour, now.minute, wakeupHour, wakeupMinute)) {
        if (!isCurrentTimeInWakeupWindow(now.hour, now.minute, wakeupHour, wakeupMinute)) {
          startBedtime();
          s_state.fadeInActive = false;  // Pas de fade-in au boot, affichage direct
          uint8_t brightnessValue = LEDManager::brightnessPercentTo255(config.brightness);
          LEDManager::setBrightness(brightnessValue);
          s_state.lastTriggeredHour = config.schedules[dayIndex].hour;
          s_state.lastTriggeredMinute = config.schedules[dayIndex].minute;
        }
      }
    }
    
  } else {
    s_state.checkingEnabled = false;
    s_state.lastCheckedDay = 0;
    s_state.lastCheckTime = millis();
  }
  
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
  
  return true;
}

bool BedtimeManager::reloadConfig() {
  // Réinitialiser les flags de déclenchement pour permettre un nouveau déclenchement
  s_state.lastTriggeredHour = 255;
  s_state.lastTriggeredMinute = 255;
  
  bool result = loadConfig();
  
  // Si la config a changé, vérifier si la routine est activée pour aujourd'hui
  if (result && s_state.initialized && RTCManager::isAvailable()) {
    if (configChanged()) {
      updateCheckingState();
      s_state.lastCheckTime = millis();
      if (s_state.checkingEnabled) {
        checkNow();
      }
    } else {
      // Config identique, juste vérifier maintenant si déjà en cours de vérification
      if (s_state.checkingEnabled) {
        // Réinitialiser quand même s_state.lastCheckTime pour recalculer l'intervalle (au cas où l'heure aurait changé)
        s_state.lastCheckTime = millis();
        checkNow();
      }
    }
  }
  
  return result;
}

void BedtimeManager::checkNow() {
  if (!s_state.initialized || !RTCManager::isAvailable()) {
    return;
  }
  // Vérifier immédiatement si c'est l'heure de déclencher le bedtime
  checkBedtimeTrigger();
}

void BedtimeManager::parseWeekdaySchedule(const char* jsonStr) {
  // Utiliser ScheduleParser pour parser le JSON weekdaySchedule
  // validateFully=true pour exiger que hour ET minute soient valides (bedtime est strict)
  ScheduleParser::parseWeekdaySchedule(jsonStr, config.schedules, true, "[BEDTIME]");
}

/**
 * Helper privé pour appliquer l'effet et la couleur bedtime aux LEDs
 * Utilisé par startBedtime() et restoreDisplayFromConfig()
 */
static void applyBedtimeDisplay(const BedtimeConfig& cfg) {
  LEDEffect effect = LEDEffectParser::parse(cfg.effect);
  bool useEffect = (effect != LED_EFFECT_NONE && strlen(cfg.effect) > 0 && strcmp(cfg.effect, "none") != 0);

  LEDManager::setEffect(useEffect ? effect : LED_EFFECT_NONE);
  LEDManager::setColor(cfg.colorR, cfg.colorG, cfg.colorB);
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
  if (!doc[WEEKDAY_NAMES[dayIndex]].is<JsonObject>()) {
    return false;
  }
  JsonObject daySchedule = doc[WEEKDAY_NAMES[dayIndex]].as<JsonObject>();
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
  if (index < 7) {
    return WEEKDAY_NAMES[index];
  }
  return WEEKDAY_NAMES[0];
}

void BedtimeManager::update() {
  if (!s_state.initialized) {
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
    // Si on n'est pas en bedtime mais qu'on est déjà dans la plage coucher->lever (ex: RTC sync après init), activer bedtime sauf si dans fenêtre wakeup
    if (!s_state.routineActive && !manuallyStarted && s_state.checkingEnabled) {
      DateTime now = RTCManager::getDateTime();
      uint8_t dayIndex = weekdayToIndex(now.dayOfWeek);
      int wakeupHour = 7, wakeupMinute = 0;
      if (getWakeupScheduleForDay(dayIndex, wakeupHour, wakeupMinute) &&
          isCurrentTimeBetweenBedtimeAndWakeup(dayIndex, now.hour, now.minute, wakeupHour, wakeupMinute) &&
          !isCurrentTimeInWakeupWindow(now.hour, now.minute, wakeupHour, wakeupMinute)) {
        startBedtime();
        s_state.fadeInActive = false;
        uint8_t brightnessValue = LEDManager::brightnessPercentTo255(config.brightness);
        LEDManager::setBrightness(brightnessValue);
        s_state.lastTriggeredHour = config.schedules[dayIndex].hour;
        s_state.lastTriggeredMinute = config.schedules[dayIndex].minute;
      } else {
        checkBedtimeTrigger();
      }
    } else {
      checkBedtimeTrigger();
    }
  }
  
  // Ne pas écraser le feedback alerte (vert/rouge pulsé)
#ifdef HAS_TOUCH
  if (DreamTouchHandler::s_alertFeedbackUntil > 0 && currentTime < DreamTouchHandler::s_alertFeedbackUntil) {
    return;
  }
#endif

  // Mettre à jour les animations de fade si actives
  if (s_state.fadeInActive) {
    updateFadeIn();
  }
  
  // Vérifier si on doit démarrer le fade-out (30 minutes après le début si allNight = false)
  if (s_state.routineActive && !config.allNight && !s_state.fadeInActive && !s_state.fadeOutActive) {
    unsigned long elapsedSinceStart;
    
    // Gérer le wrap-around de millis() (se produit après ~49 jours)
    if (currentTime >= s_state.startTime) {
      elapsedSinceStart = currentTime - s_state.startTime;
    } else {
      // Wrap-around détecté
      elapsedSinceStart = (ULONG_MAX - s_state.startTime) + currentTime;
    }
    
    if (elapsedSinceStart >= BEDTIME_DURATION_MS) {
      s_state.fadeOutActive = true;
      s_state.fadeStartTime = currentTime;
    }
  }
  
  if (s_state.fadeOutActive) {
    updateFadeOut();
  }
}

void BedtimeManager::updateCheckingState() {
  if (!RTCManager::isAvailable()) {
    s_state.checkingEnabled = false;
    return;
  }
  
  DateTime now = RTCManager::getDateTime();
  uint8_t dayIndex = weekdayToIndex(now.dayOfWeek);
  
  // Vérifier si la routine est activée pour aujourd'hui
  bool wasEnabled = s_state.checkingEnabled;
  s_state.checkingEnabled = config.schedules[dayIndex].activated;
  
  if (s_state.checkingEnabled) {
    if (!wasEnabled) {
      s_state.lastCheckTime = millis();
    }
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

  // Retourner l'intervalle en cache si le jour n'a pas changé
  if (now.dayOfWeek == s_state.lastCachedIntervalDay && !configChanged()) {
    return s_state.lastCachedCheckInterval;
  }

  // Jour changé ou config changée : recalculer
  s_state.lastCachedIntervalDay = now.dayOfWeek;

  if (!config.schedules[dayIndex].activated) {
    s_state.lastCachedCheckInterval = CHECK_INTERVAL_3H_MS;
    return s_state.lastCachedCheckInterval;
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
    s_state.lastCachedCheckInterval = CHECK_INTERVAL_3H_MS;
  } else if (hoursUntilTarget > 3.0f) {
    s_state.lastCachedCheckInterval = CHECK_INTERVAL_1H_MS;
  } else if (hoursUntilTarget > 1.0f) {
    s_state.lastCachedCheckInterval = CHECK_INTERVAL_30M_MS;
  } else {
    s_state.lastCachedCheckInterval = CHECK_INTERVAL_MS;
  }

  return s_state.lastCachedCheckInterval;
}

void BedtimeManager::checkBedtimeTrigger() {
  DateTime now = RTCManager::getDateTime();
  uint8_t dayIndex = weekdayToIndex(now.dayOfWeek);
  
  if (!config.schedules[dayIndex].activated) {
    if (s_state.routineActive) {
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
    if (!s_state.routineActive &&
        !manuallyStarted &&
        (s_state.lastTriggeredHour != now.hour || s_state.lastTriggeredMinute != now.minute)) {
      Serial.println("[BEDTIME] >>> DÉCLENCHEMENT DU BEDTIME <<<");
      startBedtime();
      s_state.lastTriggeredHour = now.hour;
      s_state.lastTriggeredMinute = now.minute;
    } else {
      if (s_state.routineActive) {
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
        !s_state.routineActive && !manuallyStarted &&
        (s_state.lastTriggeredHour != (uint8_t)targetHour || s_state.lastTriggeredMinute != (uint8_t)targetMinute)) {
      Serial.println("[BEDTIME] >>> DÉCLENCHEMENT SÉCURITÉ (dépassement 0-2 min) <<<");
      startBedtime();
      s_state.lastTriggeredHour = targetHour;
      s_state.lastTriggeredMinute = targetMinute;
    } else {
      // Log pour comprendre pourquoi ça ne correspond pas
      Serial.printf("[BEDTIME] Heure ne correspond pas: Actuelle %02d:%02d vs Config %02d:%02d\n",
                    now.hour, now.minute, targetHour, targetMinute);

      // Si on n'est plus dans la minute de déclenchement, réinitialiser les flags
      if (s_state.lastTriggeredHour == config.schedules[dayIndex].hour &&
          s_state.lastTriggeredMinute == config.schedules[dayIndex].minute) {
        Serial.println("[BEDTIME] Sortie de la minute de déclenchement, réinitialisation des flags");
        s_state.lastTriggeredHour = 255;
        s_state.lastTriggeredMinute = 255;
      }
    }
  }
}

void BedtimeManager::startBedtime() {
  // "manual" = démarré manuellement (app ou tap) → affichage "Manuel" dans l'app
  const char* state = isManuallyStarted() ? "manual" : "started";
  ModelDreamPubNubRoutes::publishRoutineState("bedtime", state);

  s_state.routineActive = true;
  s_state.startTime = millis();
  s_state.fadeInActive = true;
  s_state.fadeOutActive = false;
  s_state.fadeStartTime = millis();
  
  // Convertir brightness de 0-100 vers 0-255
  uint8_t brightnessValue = LEDManager::brightnessPercentTo255(config.brightness);
  
  // Empêcher le sleep mode pendant le bedtime
  LEDManager::preventSleep();

  // Réveiller les LEDs
  LEDManager::wakeUp();
  
  // Appliquer l'effet et la couleur selon la config bedtime
  applyBedtimeDisplay(config);
}

void BedtimeManager::updateFadeIn() {
  unsigned long elapsed = millis() - s_state.fadeStartTime;
  
  if (elapsed >= FADE_IN_DURATION_MS) {
    // Fade-in terminé
    s_state.fadeInActive = false;

    // Appliquer la brightness finale
    uint8_t brightnessValue = LEDManager::brightnessPercentTo255(config.brightness);
    if (!LEDManager::setBrightness(brightnessValue)) {
      Serial.printf("[BEDTIME] WARN: setBrightness(%d) failed\n", brightnessValue);
    }

    Serial.println("[BEDTIME] Fade-in termine");
  } else {
    // Interpolation linéaire de la brightness
    float progress = (float)elapsed / (float)FADE_IN_DURATION_MS;
    uint8_t targetBrightness = LEDManager::brightnessPercentTo255(config.brightness);
    uint8_t currentBrightness = (uint8_t)(progress * targetBrightness);

    if (!LEDManager::setBrightness(currentBrightness)) {
      Serial.printf("[BEDTIME] WARN: setBrightness(%d) failed\n", currentBrightness);
    }
  }
}

void BedtimeManager::updateFadeOut() {
  unsigned long elapsed = millis() - s_state.fadeStartTime;

  if (elapsed >= FADE_OUT_DURATION_MS) {
    // Fade-out terminé, éteindre complètement et arrêter le bedtime
    s_state.fadeOutActive = false;
    if (!LEDManager::clear()) {
      Serial.println("[BEDTIME] WARN: clear() failed");
    }
    ModelDreamPubNubRoutes::publishRoutineState("bedtime", "stopped");
    s_state.routineActive = false;
    manuallyStarted = false;
  } else {
    // Interpolation linéaire de la brightness vers 0
    float progress = (float)elapsed / (float)FADE_OUT_DURATION_MS;
    uint8_t startBrightness = LEDManager::brightnessPercentTo255(config.brightness);
    uint8_t currentBrightness = (uint8_t)(startBrightness * (1.0f - progress));

    if (!LEDManager::setBrightness(currentBrightness)) {
      Serial.printf("[BEDTIME] WARN: setBrightness(%d) failed\n", currentBrightness);
    }
  }
}

void BedtimeManager::stopBedtime(bool clearDisplay) {
  if (s_state.routineActive) {
    ModelDreamPubNubRoutes::publishRoutineState("bedtime", "stopped");
  }

  s_state.routineActive = false;
  s_state.fadeInActive = false;
  s_state.fadeOutActive = false;
  manuallyStarted = false; // Réinitialiser le flag manuel

  // Réautoriser le sleep mode
  LEDManager::allowSleep();
  
  // Éteindre les LEDs sauf si transition vers wakeup (on garde l'affichage pour le fade progressif)
  if (clearDisplay) {
    LEDManager::clear();
  }
}

bool BedtimeManager::isBedtimeEnabled() {
  if (!s_state.initialized) {
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
  return s_state.routineActive;
}

bool BedtimeManager::isManuallyStarted() {
  return manuallyStarted;
}

void BedtimeManager::startBedtimeManually() {
  // Si le bedtime est déjà marqué actif (même si les LEDs ne le reflètent pas), arrêter proprement
  // pour repartir sur une base saine et réappliquer la config
  if (s_state.routineActive || s_state.fadeInActive || s_state.fadeOutActive) {
    stopBedtime();
  }
  
  // Marquer comme démarré manuellement pour empêcher le déclenchement automatique
  manuallyStarted = true;
  
  // Démarrer le bedtime selon la config (LEDs, effet)
  startBedtime();
  
  Serial.printf("[BEDTIME] startBedtimeManually: bedtimeActive=%d manuallyStarted=%d\n",
    s_state.routineActive, manuallyStarted);
  
  // Allumage direct sans fade quand démarrage manuel (tap ou app)
  s_state.fadeInActive = false;
  uint8_t brightnessValue = LEDManager::brightnessPercentTo255(config.brightness);
  LEDManager::setBrightness(brightnessValue);
}

void BedtimeManager::stopBedtimeManually() {
  Serial.println("[BEDTIME] Arrêt manuel du bedtime");
  
  // Arrêter le bedtime (qui réinitialisera aussi manuallyStarted)
  stopBedtime();
}

void BedtimeManager::restoreDisplayFromConfig() {
  if (!s_state.initialized) {
    return;
  }
  // Ne pas écraser si on est en extinction progressive (allNight = false, fade-out en cours)
  if (s_state.fadeOutActive) {
    return;
  }
  // Réafficher effet, couleur et luminosité selon la config (sans toucher à bedtimeActive/fade)
  LEDManager::preventSleep();
  LEDManager::wakeUp();

  applyBedtimeDisplay(config);
  uint8_t brightnessValue = LEDManager::brightnessPercentTo255(config.brightness);
  LEDManager::setBrightness(brightnessValue);
}
