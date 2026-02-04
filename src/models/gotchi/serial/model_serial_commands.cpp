#include "model_serial_commands.h"
#include <Arduino.h>
#include "../../model_config.h"
#ifdef HAS_LCD
#include "../managers/emotions/emotion_manager.h"
#endif

/**
 * Commandes Serial spécifiques au modèle Kidoo Gotchi
 */

#ifdef HAS_LCD
static bool cmdEmotion(const String& args) {
  if (args.length() == 0) {
    Emotion e = EmotionManager::getEmotion();
    Serial.println("[GOTCHI] Emotion actuelle: " + String((uint8_t)e));
    Serial.println("[GOTCHI] Liste: happy(0), sad(1), hungry(2), sleepy(3), sick(4), angry(5), neutral(6)");
    return true;
  }
  String a = args;
  a.toLowerCase();
  a.trim();
  Emotion e;
  if (a == "happy" || a == "0")   e = Emotion::Happy;
  else if (a == "sad" || a == "1") e = Emotion::Sad;
  else if (a == "hungry" || a == "2") e = Emotion::Hungry;
  else if (a == "sleepy" || a == "3") e = Emotion::Sleepy;
  else if (a == "sick" || a == "4")  e = Emotion::Sick;
  else if (a == "angry" || a == "5") e = Emotion::Angry;
  else if (a == "neutral" || a == "6") e = Emotion::Neutral;
  else {
    Serial.println("[GOTCHI] Emotion inconnue. Utilise: happy, sad, hungry, sleepy, sick, angry, neutral (ou 0-6)");
    return true;
  }
  EmotionManager::setEmotion(e);
  Serial.println("[GOTCHI] Emotion definie: " + String((uint8_t)e));
  return true;
}
#endif

bool ModelGotchiSerialCommands::processCommand(const String& command) {
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

  if (cmd == "gotchi-info") {
    Serial.println("[GOTCHI] Informations specifiques au modele Gotchi");
    Serial.println("[GOTCHI] ESP32-S3-N16R8 - 16MB Flash / 8MB PSRAM");
    Serial.println("[GOTCHI] Modele: Kidoo Gotchi");
    return true;
  }

#ifdef HAS_LCD
  if (cmd == "emotion") {
    return cmdEmotion(args);
  }
#endif

  return false;
}

void ModelGotchiSerialCommands::printHelp() {
  Serial.println("");
  Serial.println("========================================");
  Serial.println("  COMMANDES SPECIFIQUES GOTCHI");
  Serial.println("========================================");
  Serial.println("  gotchi-info    - Afficher les infos du modele Gotchi");
#ifdef HAS_LCD
  Serial.println("  emotion        - Afficher l'emotion actuelle et la liste");
  Serial.println("  emotion <nom>  - Definir l'emotion (happy,sad,hungry,sleepy,sick,angry,neutral)");
  Serial.println("  emotion <0-6>  - Definir par index (0=happy, 6=neutral)");
#endif
  Serial.println("========================================");
  Serial.println("");
}
