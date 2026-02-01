#include "model_serial_commands.h"
#include "../../model_config.h"
#include "../managers/bedtime/bedtime_manager.h"
#include "../managers/wakeup/wakeup_manager.h"
#include "../../common/managers/led/led_manager.h"
#include <Arduino.h>

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
    Serial.println("[DREAM] Nombre de LEDs: 40");
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
    // Commande: rainbow on | rainbow off
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
    else {
      Serial.println("[DREAM] Usage: rainbow on | rainbow off");
      Serial.println("[DREAM]   Active ou desactive l'effet arc-en-ciel doux (animation lente et apaisante)");
      return true;
    }
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
  Serial.println("  nightlight on      - Activer l'effet veilleuse (vagues bleu/blanc)");
  Serial.println("  nightlight off     - Desactiver l'effet veilleuse");
  Serial.println("  rainbow on         - Activer l'effet arc-en-ciel doux (animation lente et apaisante)");
  Serial.println("  rainbow off        - Desactiver l'effet arc-en-ciel doux");
  Serial.println("  breathe on         - Activer l'effet respiration (respiration avec changement de couleur)");
  Serial.println("  breathe off        - Desactiver l'effet respiration");
  Serial.println("========================================");
  Serial.println("");
}
