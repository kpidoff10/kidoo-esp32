#ifndef DREAM_RTC_MACROS_H
#define DREAM_RTC_MACROS_H

#include "common/managers/rtc/rtc_manager.h"
#include <Arduino.h>

/**
 * Macros pour standardiser les vérifications RTC dans BedtimeManager et WakeupManager.
 * Réduit la duplication et assure un message d'erreur cohérent.
 */

/** Alias principal : vérifie RTC, sinon log et return false */
#define RTC_CHECK_OR_RETURN(tag) RTC_CHECK_OR_RETURN_FALSE(tag)

/** Vérifie RTC disponible, sinon log et return false (pour init) */
#define RTC_CHECK_OR_RETURN_FALSE(tag) \
  do { \
    if (!RTCManager::isAvailable()) { \
      Serial.println("[" tag "] ERREUR: RTC non disponible"); \
      return false; \
    } \
  } while (0)

/** Vérifie RTC disponible, sinon log et return void (pour update, checkNow) */
#define RTC_CHECK_OR_RETURN_VOID(tag) \
  do { \
    if (!RTCManager::isAvailable()) { \
      Serial.println("[" tag "] ERREUR: RTC non disponible"); \
      return; \
    } \
  } while (0)

#endif  // DREAM_RTC_MACROS_H
