#include "model_serial_commands.h"
#include <Arduino.h>
#include <Wire.h>
#include "../lvgl/gotchi_lvgl.h"
#include "../face/face_engine.h"
#include "../face/face_config.h"
#include "../face/behavior/behavior_engine.h"
#include "../config/gotchi_theme.h"
#include "../config/config.h"

bool ModelGotchiSerialCommands::processCommand(const String& command) {
  if (command == "gotchi-info") {
    Serial.println("[GOTCHI] Waveshare ESP32-S3-Touch-AMOLED-1.75 (466x466, QSPI)");
    return true;
  }

  // Scan I2C sur Wire1 (NFC) et Wire (principal)
  if (command == "i2c scan") {
    // Wire1 — NFC bus
    Serial.printf("[I2C] Scan Wire1 (SDA=%d, SCL=%d):\n", NFC_SDA_PIN, NFC_SCL_PIN);
    Wire1.begin(NFC_SDA_PIN, NFC_SCL_PIN);
    delay(100);
    int found1 = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
      Wire1.beginTransmission(addr);
      if (Wire1.endTransmission() == 0) {
        Serial.printf("  0x%02X\n", addr);
        found1++;
      }
    }
    if (!found1) Serial.println("  (aucun device)");

    // Wire — bus principal
    Serial.printf("[I2C] Scan Wire (SDA=%d, SCL=%d):\n", IIC_SDA, IIC_SCL);
    int found0 = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0) {
        Serial.printf("  0x%02X\n", addr);
        found0++;
      }
    }
    if (!found0) Serial.println("  (aucun device)");
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

    // --- Jouer ---
    if (arg == "play ball" || arg == "play") {
      BehaviorEngine::tryPlay();
      return true;
    }

    // --- Événements ---
    if (arg == "touch") { BehaviorEngine::onTouch(); return true; }
    if (arg == "shake") { BehaviorEngine::onShake(); return true; }
    if (arg == "pet")   { BehaviorEngine::onPet();   return true; }
    if (arg == "sound") { BehaviorEngine::onSound(); return true; }

    // --- Set stats (face set <stat> <value>) ---
    if (arg.startsWith("set ")) {
      String rest = arg.substring(4); rest.trim();
      int sp = rest.indexOf(' ');
      if (sp > 0) {
        String stat = rest.substring(0, sp); stat.trim();
        float val = rest.substring(sp + 1).toFloat();
        auto& s = BehaviorEngine::getStats();
        if (stat == "hunger")         s.hunger = val;
        else if (stat == "energy")    s.energy = val;
        else if (stat == "happiness") s.happiness = val;
        else if (stat == "health")    s.health = val;
        else if (stat == "hygiene")   s.hygiene = val;
        else if (stat == "boredom")   s.boredom = val;
        else if (stat == "excitement") s.excitement = val;
        else if (stat == "irritability") s.irritability = val;
        else { Serial.printf("[STATS] Inconnu: %s\n", stat.c_str()); return true; }
        s.clamp();
        Serial.printf("[STATS] %s = %.0f\n", stat.c_str(), val);
      } else {
        Serial.println("[STATS] Usage: face set <stat> <value>");
        Serial.println("  Stats: hunger, energy, happiness, health, hygiene, boredom, excitement, irritability");
      }
      return true;
    }

    // --- Reset all stats ---
    if (arg == "reset") {
      BehaviorEngine::getStats() = BehaviorStats();
      Serial.println("[STATS] Reset to defaults");
      return true;
    }

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

    // --- Theme / sexe ---
    if (arg == "boy") {
      GotchiTheme::setPreset(GotchiTheme::Preset::Boy);
      GotchiTheme::saveToConfig();
      return true;
    }
    if (arg == "girl") {
      GotchiTheme::setPreset(GotchiTheme::Preset::Girl);
      GotchiTheme::saveToConfig();
      return true;
    }
    if (arg.startsWith("theme ")) {
      String t = arg.substring(6); t.trim();
      GotchiTheme::setPresetByName(t.c_str());
      GotchiTheme::saveToConfig();
      return true;
    }

    // --- Face engine direct ---
    if (arg == "blink") { FaceEngine::blink(); return true; }

    // Gestes : nod (oui) et shake (non) avec vitesse optionnelle
    if (arg.startsWith("nod")) {
      String spd = arg.substring(3); spd.trim();
      if (spd == "slow")      FaceEngine::nod(FaceEngine::GestureSpeed::Slow);
      else if (spd == "fast") FaceEngine::nod(FaceEngine::GestureSpeed::Fast);
      else                    FaceEngine::nod(FaceEngine::GestureSpeed::Normal);
      return true;
    }
    if (arg.startsWith("no")) {
      String spd = arg.substring(2); spd.trim();
      if (spd == "slow")      FaceEngine::shake(FaceEngine::GestureSpeed::Slow);
      else if (spd == "fast") FaceEngine::shake(FaceEngine::GestureSpeed::Fast);
      else                    FaceEngine::shake(FaceEngine::GestureSpeed::Normal);
      return true;
    }

    if (arg.startsWith("look ")) {
      String coords = arg.substring(5);
      int sp = coords.indexOf(' ');
      if (sp > 0) {
        FaceEngine::lookAt(coords.substring(0, sp).toFloat(), coords.substring(sp + 1).toFloat());
      }
      return true;
    }

    // --- Expression directe (désactive le behavior auto) ---
    BehaviorEngine::setAutoMode(false);
    FaceExpression expr = FacePresets::parseExpression(arg.c_str());
    FaceEngine::setExpression(expr);
    Serial.printf("[FACE] Expression forcée: %s (auto OFF)\n", arg.c_str());
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
  Serial.println("  face play ball               Jouer a la balle (swipe ecran = lancer)");
  Serial.println("  face set <stat> <value>      Modifier une stat (hunger, energy, happiness, health,");
  Serial.println("                               hygiene, boredom, excitement, irritability)");
  Serial.println("  face reset                   Reset toutes les stats");
  Serial.println("  face touch                   Taper");
  Serial.println("  face pet                     Caresser");
  Serial.println("  face shake                   Secouer");
  Serial.println("  face sound                   Son");
  Serial.println("  === Theme ===");
  Serial.println("  face boy                     Theme garcon (cyan)");
  Serial.println("  face girl                    Theme fille (magenta)");
  Serial.println("  face theme <name>            green, gold, red, white");
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
