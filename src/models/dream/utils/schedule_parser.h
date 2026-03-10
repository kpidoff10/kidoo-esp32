#ifndef SCHEDULE_PARSER_H
#define SCHEDULE_PARSER_H

#include <ArduinoJson.h>
#include "models/dream/config/dream_config.h"
#include "models/dream/managers/dream_schedules.h"

/**
 * Parser pour les schedules de bedtime/wakeup au format JSON weekday
 * Consolide la logique de parsing présente dans BedtimeManager et WakeupManager
 */
class ScheduleParser {
public:
  /**
   * Parser un JSON weekdaySchedule et mettre à jour les schedules du config
   *
   * Format JSON attendu:
   * {
   *   "monday": { "hour": 20, "minute": 0, "activated": true },
   *   "tuesday": { "hour": 20, "minute": 30, "activated": false },
   *   ...
   * }
   *
   * @param jsonStr String JSON à parser
   * @param schedules Tableau des 7 schedules à remplir
   * @param validateFully Si true, exige que hour et minute soient valides pour activer
   * @param debugPrefix Préfixe pour les messages de debug (ex: "[BEDTIME]")
   */
  static void parseWeekdaySchedule(
    const char* jsonStr,
    DreamSchedule* schedules,
    bool validateFully = true,
    const char* debugPrefix = nullptr
  ) {
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
      if (debugPrefix) {
        Serial.print(debugPrefix);
        Serial.print(" ERREUR parsing weekdaySchedule: ");
        Serial.println(error.c_str());
      }
      return;
    }

    // Parser chaque jour (accepter hour/minute en int ou double pour compatibilité JSON)
    for (int i = 0; i < 7; i++) {
      if (doc[WEEKDAY_NAMES[i]].is<JsonObject>()) {
        JsonObject daySchedule = doc[WEEKDAY_NAMES[i]].as<JsonObject>();
        int h = -1, m = -1;

        // Parser hour - accepter int ou double
        if (daySchedule["hour"].is<int>()) {
          h = daySchedule["hour"].as<int>();
        } else if (daySchedule["hour"].is<double>()) {
          h = (int)daySchedule["hour"].as<double>();
        }

        // Parser minute - accepter int ou double
        if (daySchedule["minute"].is<int>()) {
          m = daySchedule["minute"].as<int>();
        } else if (daySchedule["minute"].is<double>()) {
          m = (int)daySchedule["minute"].as<double>();
        }

        // Valider et copier les valeurs
        if (h >= 0 && h <= 23) {
          schedules[i].hour = (uint8_t)h;
        }
        if (m >= 0 && m <= 59) {
          schedules[i].minute = (uint8_t)m;
        }

        // Déterminer si activé
        if (daySchedule["activated"].is<bool>()) {
          schedules[i].activated = daySchedule["activated"].as<bool>();
        } else {
          // Si pas de champ "activated" explicite:
          // - Si validateFully: exiger que h ET m soient valides
          // - Si !validateFully: seulement h et m doivent être >= 0
          if (validateFully) {
            schedules[i].activated = (h >= 0 && h <= 23 && m >= 0 && m <= 59);
          } else {
            schedules[i].activated = (h >= 0 && m >= 0);
          }
        }
      }
    }
  }
};

#endif // SCHEDULE_PARSER_H
