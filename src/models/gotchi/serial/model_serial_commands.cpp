#include "model_serial_commands.h"
#include <Arduino.h>
#include "../lvgl/gotchi_lvgl.h"
#include "../face/face_engine.h"
#include "../face/face_config.h"
#include "../face/behavior/behavior_engine.h"

bool ModelGotchiSerialCommands::processCommand(const String& command) {
  if (command == "gotchi-info") {
    Serial.println("[GOTCHI] Waveshare ESP32-S3-Touch-AMOLED-1.75 (466x466, QSPI)");
    return true;
  }
  if (command == "gotchi-test") {
    GotchiLvgl::testDisplay();
    return true;
  }

  if (command.startsWith("face ")) {
    String arg = command.substring(5);
    arg.trim();

    // --- Actions de vie ---
    if (arg.startsWith("feed ")) {
      String food = arg.substring(5);
      food.trim();
      BehaviorEngine::feed(food.c_str());
      return true;
    }
    if (arg == "feed") {
      BehaviorEngine::feed("apple"); // Défaut
      return true;
    }
    if (arg == "medicine") {
      BehaviorEngine::heal();
      return true;
    }
    if (arg == "clean") {
      BehaviorEngine::clean();
      return true;
    }

    // --- Événements ---
    if (arg == "touch") { BehaviorEngine::onTouch(); return true; }
    if (arg == "shake") { BehaviorEngine::onShake(); return true; }
    if (arg == "sound") { BehaviorEngine::onSound(); return true; }

    // --- Stats ---
    if (arg == "stats") {
      auto& s = BehaviorEngine::getStats();
      Serial.println("╔══════════════════════════════════════╗");
      Serial.printf( "║  Behavior: %-10s  Need: %-8s ║\n",
        BehaviorEngine::getCurrentBehavior(), needToString(BehaviorEngine::getCurrentNeed()));
      Serial.printf( "║  Age: %lu min  Auto: %s            ║\n",
        s.ageMinutes, BehaviorEngine::isAutoMode() ? "ON " : "OFF");
      Serial.println("╠══════════════════════════════════════╣");
      Serial.printf( "║  Hunger:    %5.1f / 100  %s        ║\n", s.hunger,
        s.hunger < 25 ? "⚠ LOW" : "     ");
      Serial.printf( "║  Energy:    %5.1f / 100  %s        ║\n", s.energy,
        s.energy < 15 ? "⚠ LOW" : "     ");
      Serial.printf( "║  Happiness: %5.1f / 100  %s        ║\n", s.happiness,
        s.happiness < 20 ? "⚠ LOW" : "     ");
      Serial.printf( "║  Health:    %5.1f / 100  %s        ║\n", s.health,
        s.health < 30 ? "⚠ LOW" : "     ");
      Serial.printf( "║  Hygiene:   %5.1f / 100  %s        ║\n", s.hygiene,
        s.hygiene < 20 ? "⚠ LOW" : "     ");
      Serial.println("╠══════════════════════════════════════╣");
      Serial.printf( "║  Boredom:   %5.1f  Excitement: %5.1f ║\n", s.boredom, s.excitement);
      Serial.printf( "║  Mouth:     %5.2f                   ║\n", s.mouthState);
      Serial.println("╚══════════════════════════════════════╝");
      return true;
    }

    // --- Behaviors ---
    if (arg.startsWith("behavior ")) {
      String name = arg.substring(9);
      name.trim();
      if (name == "auto") {
        BehaviorEngine::setAutoMode(true);
      } else {
        BehaviorEngine::forceState(name.c_str());
      }
      return true;
    }

    // --- Face engine direct ---
    if (arg == "blink") { FaceEngine::blink(); return true; }

    if (arg.startsWith("look ")) {
      String coords = arg.substring(5);
      int sp = coords.indexOf(' ');
      if (sp > 0) {
        FaceEngine::lookAt(coords.substring(0, sp).toFloat(), coords.substring(sp + 1).toFloat());
      }
      return true;
    }

    // --- Expression directe ---
    FaceExpression expr = FacePresets::parseExpression(arg.c_str());
    FaceEngine::setExpression(expr);
    return true;
  }

  return false;
}

void ModelGotchiSerialCommands::printHelp() {
  Serial.println("  === Gotchi ===");
  Serial.println("  gotchi-info                  Infos matériel");
  Serial.println("  gotchi-test                  Test écran AMOLED");
  Serial.println("  === Vie ===");
  Serial.println("  face feed [bottle|cake|apple|candy]  Nourrir");
  Serial.println("  face medicine                Soigner");
  Serial.println("  face clean                   Nettoyer");
  Serial.println("  face touch                   Caresser");
  Serial.println("  face shake                   Secouer");
  Serial.println("  face sound                   Son");
  Serial.println("  === Infos ===");
  Serial.println("  face stats                   Stats complètes");
  Serial.println("  === Behaviors ===");
  Serial.println("  face behavior auto           Mode autonome");
  Serial.println("  face behavior <name>         Force (idle,play,sleep,sad,happy,");
  Serial.println("                               hungry,eating,sick,dirty,lonely,curious,tantrum)");
  Serial.println("  === Face ===");
  Serial.println("  face <expression>            Force expression");
  Serial.println("  face look <x> <y>            Regard (-1 à 1)");
  Serial.println("  face blink                   Clignement");
}
