#include "model_serial_commands.h"
#include "models/model_config.h"
#include "models/model_pubnub_routes.h"
#include "common/managers/rtc/rtc_manager.h"
#include "models/dream/config/dream_config.h"
#include "models/dream/managers/bedtime/bedtime_manager.h"
#include "models/dream/managers/wakeup/wakeup_manager.h"
#include "models/dream/managers/touch/dream_touch_handler.h"
#include "models/dream/api/dream_api_routes.h"
#include "common/managers/led/led_manager.h"
#include "common/managers/wifi/wifi_manager.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#ifdef HAS_WIFI
#include <WiFi.h>
#endif

/**
 * Commandes Serial spécifiques au modèle Kidoo Dream
 */

bool ModelDreamSerialCommands::processCommand(const String& command) {
  // Séparer la commande et les arguments
  int spaceIndex = command.indexOf(' ');
  String cmd = command;
  String args = "";
  
  if (spaceIndex > 0) {
    cmd = command.substring(0, spaceIndex);
    args = command.substring(spaceIndex + 1);
  }
  
  cmd.toLowerCase();
  cmd.trim();
  args.trim();
  
  // Traiter les commandes spécifiques au Dream
  if (cmd == "dream-info") {
    Serial.println("[DREAM] Informations specifiques au modele Dream");
#ifdef NUM_LEDS
    Serial.printf("[DREAM] Nombre de LEDs: %d\n", NUM_LEDS);
#else
    Serial.println("[DREAM] Nombre de LEDs: (non defini)");
#endif
    Serial.println("[DREAM] Modele: Kidoo Dream");
    Serial.println("[DREAM] NFC: Non disponible");
    return true;
  }
  else if (cmd == "bedtime-show" || cmd == "show-bedtime") {
    BedtimeConfig config = BedtimeManager::getConfig();
    
    Serial.println("");
    Serial.println("========================================");
    Serial.println("  CONFIGURATION BEDTIME (COUCHER)");
    Serial.println("========================================");
    Serial.printf("Couleur: RGB(%d, %d, %d)\n", config.colorR, config.colorG, config.colorB);
    Serial.printf("Luminosite: %d%%\n", config.brightness);
    Serial.printf("Allume toute la nuit: %s\n", config.allNight ? "Oui" : "Non");
    Serial.println("");
    Serial.println("Horaires par jour:");
    
    const char* weekdays[] = {"Lundi", "Mardi", "Mercredi", "Jeudi", "Vendredi", "Samedi", "Dimanche"};
    bool hasAnySchedule = false;
    
    for (int i = 0; i < 7; i++) {
      if (config.schedules[i].activated) {
        Serial.printf("  %s: %02d:%02d (Active)\n", 
                     weekdays[i], 
                     config.schedules[i].hour, 
                     config.schedules[i].minute);
        hasAnySchedule = true;
      } else {
        Serial.printf("  %s: %02d:%02d (Inactif)\n", 
                     weekdays[i], 
                     config.schedules[i].hour, 
                     config.schedules[i].minute);
      }
    }
    
    if (!hasAnySchedule) {
      Serial.println("  Aucun horaire active");
    }
    
    // Diagnostic: jour détecté par le RTC et routine activée pour aujourd'hui
    if (RTCManager::isAvailable()) {
      DateTime now = RTCManager::getDateTime();
      uint8_t dayIndex = (now.dayOfWeek >= 1 && now.dayOfWeek <= 7) ? (now.dayOfWeek - 1) : 0;
      Serial.println("");
      Serial.printf("Aujourd'hui (RTC): %s (dayOfWeek=%d)\n", weekdays[dayIndex], now.dayOfWeek);
      Serial.printf("Routine coucher active pour aujourd'hui: %s\n", config.schedules[dayIndex].activated ? "Oui" : "Non");
    } else {
      Serial.println("");
      Serial.println("RTC non disponible - impossible de verifier le jour");
    }
    
    Serial.printf("Bedtime actif: %s\n", BedtimeManager::isBedtimeActive() ? "Oui" : "Non");
    Serial.println("========================================");
    Serial.println("");
    
    return true;
  }
  else if (cmd == "wakeup-show" || cmd == "show-wakeup") {
    WakeupConfig config = WakeupManager::getConfig();
    
    Serial.println("");
    Serial.println("========================================");
    Serial.println("  CONFIGURATION WAKEUP (REVEIL)");
    Serial.println("========================================");
    Serial.printf("Couleur: RGB(%d, %d, %d)\n", config.colorR, config.colorG, config.colorB);
    Serial.printf("Luminosite: %d%%\n", config.brightness);
    Serial.println("");
    Serial.println("Horaires par jour:");
    Serial.println("(Le reveil commence 15 minutes avant l'heure indiquee)");
    
    const char* weekdays[] = {"Lundi", "Mardi", "Mercredi", "Jeudi", "Vendredi", "Samedi", "Dimanche"};
    bool hasAnySchedule = false;
    
    for (int i = 0; i < 7; i++) {
      if (config.schedules[i].activated) {
        // Calculer l'heure de début (15 minutes avant)
        uint8_t startHour = config.schedules[i].hour;
        uint8_t startMinute = config.schedules[i].minute;
        
        if (startMinute >= 15) {
          startMinute -= 15;
        } else {
          startMinute += 45;
          if (startHour > 0) {
            startHour--;
          } else {
            startHour = 23;
          }
        }
        
        Serial.printf("  %s: %02d:%02d (Active - demarre a %02d:%02d)\n", 
                     weekdays[i], 
                     config.schedules[i].hour, 
                     config.schedules[i].minute,
                     startHour,
                     startMinute);
        hasAnySchedule = true;
      } else {
        Serial.printf("  %s: %02d:%02d (Inactif)\n", 
                     weekdays[i], 
                     config.schedules[i].hour, 
                     config.schedules[i].minute);
      }
    }
    
    if (!hasAnySchedule) {
      Serial.println("  Aucun horaire active");
    }
    
    Serial.printf("Wakeup actif: %s\n", WakeupManager::isWakeupActive() ? "Oui" : "Non");
    Serial.println("========================================");
    Serial.println("");
    
    return true;
  }
  else if (cmd == "nightlight" || cmd == "veilleuse") {
    // Commande: nightlight on | nightlight off
    if (args == "on" || args == "enable" || args == "start") {
      Serial.println("[DREAM] Activation de l'effet veilleuse");
      LEDManager::wakeUp();
      LEDManager::setEffect(LED_EFFECT_NIGHTLIGHT);
      Serial.println("[DREAM] Effet veilleuse active (vagues bleu/blanc)");
      return true;
    }
    else if (args == "off" || args == "disable" || args == "stop") {
      Serial.println("[DREAM] Desactivation de l'effet veilleuse");
      LEDManager::setEffect(LED_EFFECT_NONE);
      LEDManager::clear();
      Serial.println("[DREAM] Effet veilleuse desactive");
      return true;
    }
    else {
      Serial.println("[DREAM] Usage: nightlight on | nightlight off");
      Serial.println("[DREAM]   Active ou desactive l'effet de veilleuse (vagues bleu/blanc)");
      return true;
    }
  }
  else if (cmd == "breathe" || cmd == "respiration") {
    // Commande: breathe on | breathe off
    if (args == "on" || args == "enable" || args == "start") {
      Serial.println("[DREAM] Activation de l'effet respiration");
      LEDManager::wakeUp();
      LEDManager::setEffect(LED_EFFECT_BREATHE);
      Serial.println("[DREAM] Effet respiration active (respiration avec changement de couleur toutes les 30s)");
      return true;
    }
    else if (args == "off" || args == "disable" || args == "stop") {
      Serial.println("[DREAM] Desactivation de l'effet respiration");
      LEDManager::setEffect(LED_EFFECT_NONE);
      LEDManager::clear();
      Serial.println("[DREAM] Effet respiration desactive");
      return true;
    }
    else {
      Serial.println("[DREAM] Usage: breathe on | breathe off");
      Serial.println("[DREAM]   Active ou desactive l'effet de respiration (respiration avec changement de couleur)");
      return true;
    }
  }
  else if (cmd == "rainbow" || cmd == "arcenciel") {
    // Commande: rainbow on | rainbow off | rainbow fast
    if (args == "on" || args == "enable" || args == "start") {
      Serial.println("[DREAM] Activation de l'effet arc-en-ciel doux (veilleuse)");
      LEDManager::wakeUp();
      LEDManager::setEffect(LED_EFFECT_RAINBOW_SOFT);
      Serial.println("[DREAM] Effet arc-en-ciel doux active (animation lente et apaisante)");
      return true;
    }
    else if (args == "off" || args == "disable" || args == "stop") {
      Serial.println("[DREAM] Desactivation de l'effet arc-en-ciel doux");
      LEDManager::setEffect(LED_EFFECT_NONE);
      LEDManager::clear();
      Serial.println("[DREAM] Effet arc-en-ciel doux desactive");
      return true;
    }
    else if (args == "fast" || args == "rapide") {
      // Test du RAINBOW rapide comme dans l'alert
      Serial.println("[DREAM] Activation de l'effet arc-en-ciel RAPIDE (comme l'alert)");
      LEDManager::preventSleep();
      LEDManager::wakeUp();
      LEDManager::setEffect(LED_EFFECT_RAINBOW);
      LEDManager::setBrightness(LEDManager::brightnessPercentTo255(80));
      Serial.println("[DREAM] Effet arc-en-ciel RAPIDE active (test de l'alert)");
      return true;
    }
    else if (args == "fast-no-wake") {
      // Test du RAINBOW rapide SANS wakeUp() pour voir si c'est le problème
      Serial.println("[DREAM] Activation de l'effet arc-en-ciel RAPIDE SANS wakeUp()");
      LEDManager::setEffect(LED_EFFECT_RAINBOW);
      LEDManager::setBrightness(LEDManager::brightnessPercentTo255(80));
      Serial.println("[DREAM] Effet arc-en-ciel RAPIDE active (SANS wakeUp)");
      return true;
    }
    else {
      Serial.println("[DREAM] Usage: rainbow on | rainbow off | rainbow fast | rainbow fast-no-wake");
      Serial.println("[DREAM]   rainbow on/off: Active ou desactive l'effet arc-en-ciel doux (animation lente et apaisante)");
      Serial.println("[DREAM]   rainbow fast: Active l'effet arc-en-ciel RAPIDE (test de l'alert nighttime-alert-ack)");
      Serial.println("[DREAM]   rainbow fast-no-wake: Active l'effet arc-en-ciel RAPIDE sans wakeUp (debug)");
      return true;
    }
  }
  else if (cmd == "alert-show" || cmd == "show-alert" || cmd == "nighttime-alert-show") {
    DreamConfig config = DreamConfigManager::getConfig();
    Serial.println("");
    Serial.println("========================================");
    Serial.println("  CONFIGURATION ALERTE NOCTURNE (DREAM)");
    Serial.println("========================================");
    Serial.printf("Alerte nocturne activee: %s\n", config.nighttime_alert_enabled ? "Oui" : "Non");
    Serial.println("  (Appui maintenu 2s sur la veilleuse = envoi notification)");
    Serial.println("========================================");
    Serial.println("");
    return true;
  }
  else if (cmd == "alert" || cmd == "send-alert" || cmd == "nighttime-alert") {
#ifdef HAS_WIFI
    Serial.println("[DREAM] Envoi alerte veilleuse au serveur...");
    bool ok = DreamApiRoutes::postNighttimeAlert();
    Serial.printf("[DREAM] Alerte %s\n", ok ? "envoyee" : "echec");
    DreamTouchHandler::triggerAlertFeedback(ok);
#else
    Serial.println("[DREAM] Alerte: WiFi non disponible");
    DreamTouchHandler::triggerAlertFeedback(false);
#endif
    return true;
  }
  else if (cmd == "nighttime-alert-ack" || cmd == "j-arrive") {
    // Simule la réception du signal "J'arrive" (envoyé par l'app quand le parent tape sur la notification)
    Serial.println("[DREAM] Simulation J'arrive (rotate rainbow 5 sec)...");
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    StaticJsonDocument<64> doc;
    #pragma GCC diagnostic pop
    doc["action"] = "nighttime-alert-ack";
    ModelPubNubRoutes::processMessage(doc.as<JsonObject>());
    return true;
  }
  return false; // Commande non reconnue
}

void ModelDreamSerialCommands::printHelp() {
  Serial.println("");
  Serial.println("========================================");
  Serial.println("  COMMANDES SPECIFIQUES DREAM");
  Serial.println("========================================");
  Serial.println("  dream-info         - Afficher les infos du modele Dream");
  Serial.println("  bedtime-show       - Afficher la configuration bedtime (coucher)");
  Serial.println("  wakeup-show        - Afficher la configuration wakeup (reveil)");
  Serial.println("  alert-show         - Afficher si l'alerte nocturne est configuree");
  Serial.println("  nightlight on      - Activer l'effet veilleuse (vagues bleu/blanc)");
  Serial.println("  nightlight off     - Desactiver l'effet veilleuse");
  Serial.println("  rainbow on         - Activer l'effet arc-en-ciel doux (animation lente et apaisante)");
  Serial.println("  rainbow off        - Desactiver l'effet arc-en-ciel doux");
  Serial.println("  breathe on         - Activer l'effet respiration (respiration avec changement de couleur)");
  Serial.println("  breathe off        - Desactiver l'effet respiration");
  Serial.println("  alert              - Envoyer alerte veilleuse (test)");
  Serial.println("  nighttime-alert-ack - Simuler J'arrive (rotate rainbow 5 sec, recu via PubNub)");
  Serial.println("========================================");
  Serial.println("");
}
