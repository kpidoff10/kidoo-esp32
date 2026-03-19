#include "model_serial_commands.h"
#include <Arduino.h>
#include "../lvgl/gotchi_lvgl.h"

bool ModelGotchiSerialCommands::processCommand(const String& command) {
  if (command == "gotchi-info") {
    Serial.println("[GOTCHI] Modèle: Waveshare ESP32-S3-Touch-AMOLED-1.75 (466x466, QSPI)");
    return true;
  }
  if (command == "gotchi-test") {
    Serial.println("[GOTCHI] Lancement test AMOLED...");
    GotchiLvgl::testDisplay();
    return true;
  }
  return false;
}

void ModelGotchiSerialCommands::printHelp() {
  Serial.println("  gotchi-info    - Infos du modèle Gotchi");
  Serial.println("  gotchi-test    - Test écran AMOLED (5 couleurs)");
}
