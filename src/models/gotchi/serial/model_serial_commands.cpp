#include "model_serial_commands.h"
#include <Arduino.h>
#include <Wire.h>
#include "../lvgl/gotchi_lvgl.h"
#include "../face/face_engine.h"
#include "../face/face_config.h"
#include "../face/behavior/behavior_engine.h"
#include "../face/behavior/poop_manager.h"
#include "../face/behavior/dirt_overlay.h"
#include "../config/gotchi_theme.h"
#include "../config/config.h"
#include "../audio/gotchi_speaker_test.h"
#include "../audio/sounds/sound_sneeze.h"
#include "common/managers/sd/sd_manager.h"
#include "common/managers/nfc/nfc_manager.h"

// Defini dans behavior_idle.cpp — declenche une scene idle pour test
extern bool idleTriggerScene(int num);

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
    if (arg == "feed bottle off" || arg == "feed off") {
      BehaviorEngine::stopFeeding();
      return true;
    }
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
    if (arg == "thermometer off") {
      BehaviorEngine::stopThermometer();
      return true;
    }
    if (arg == "thermometer") {
      BehaviorEngine::startThermometer();
      return true;
    }
    if (arg == "medicine") {
      BehaviorEngine::giveMedicine();
      return true;
    }
    if (arg == "brush off") {
      DirtOverlay::setBrushMode(false);
      return true;
    }
    if (arg == "brush") {
      DirtOverlay::setBrushMode(true);
      return true;
    }
    if (arg == "wash off") {
      DirtOverlay::clear();
      return true;
    }
    if (arg == "wash") {
      if (!DirtOverlay::isDirty()) {
        DirtOverlay::setDirty(BehaviorEngine::getStats().hygiene);
      }
      DirtOverlay::setWashMode(true);
      return true;
    }
    if (arg == "dirty") {
      DirtOverlay::setDirty(BehaviorEngine::getStats().hygiene);
      return true;
    }
    if (arg == "poop") {
      PoopManager::spawnOne();
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

    // --- Scenes idle (test des mini-animations) ---
    // Usage: face idle <1-5>
    //   1=Daydream  2=LookAround  3=EyeRoll  4=DoubleTake  5=KnockKnock
    if (arg.startsWith("idle ")) {
      int num = arg.substring(5).toInt();
      // Forcer le behavior idle d'abord (sinon les scenes ne tournent pas)
      BehaviorEngine::forceState("idle");
      if (!idleTriggerScene(num)) {
        Serial.println("[IDLE] Usage: face idle <1-25>");
        Serial.println("  1=Daydream 2=LookAround 3=EyeRoll 4=DoubleTake 5=KnockKnock");
        Serial.println("  6=Hum 7=CatchStar 8=ChaseFly 9=PeekABoo 10=Wishful");
        Serial.println("  11=Stretch 12=Sneeze 13=Dance 14=Hiccup 15=NapAttempt");
        Serial.println("  16=Yawn 17=Whistle 18=Purr 19=StomachGrowl 20=Reading");
        Serial.println("  21=Juggle 22=Bubbles 23=Airplane 24=Grimace 25=PeekABoo2");
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

  // === NFC ===
  if (command == "nfc scan") {
    if (!NFCManager::isAvailable()) {
      Serial.println("[NFC] Module non disponible");
      return true;
    }
    bool wasAuto = NFCManager::isAutoDetectEnabled();
    if (wasAuto) NFCManager::setAutoDetect(false);
    delay(100);
    Serial.println("[NFC] Approchez un tag...");
    uint8_t uid[10];
    uint8_t uidLength;
    if (NFCManager::readTagUID(uid, &uidLength, 10000)) {
      Serial.print("[NFC] Tag detecte - UID: ");
      for (uint8_t i = 0; i < uidLength; i++) {
        if (uid[i] < 0x10) Serial.print("0");
        Serial.print(uid[i], HEX);
        if (i < uidLength - 1) Serial.print(":");
      }
      Serial.println();
    } else {
      Serial.println("[NFC] Aucun tag detecte (timeout 10s)");
    }
    if (wasAuto) NFCManager::setAutoDetect(true);
    return true;
  }
  if (command == "nfc read") {
    if (!NFCManager::isAvailable()) {
      Serial.println("[NFC] Module non disponible");
      return true;
    }
    bool wasAuto = NFCManager::isAutoDetectEnabled();
    if (wasAuto) NFCManager::setAutoDetect(false);
    delay(100);
    Serial.println("[NFC] Approchez un tag pour lire le bloc 4...");
    uint8_t uid[10];
    uint8_t uidLength;
    if (NFCManager::readTagUID(uid, &uidLength, 10000)) {
      Serial.print("[NFC] UID: ");
      for (uint8_t i = 0; i < uidLength; i++) {
        if (uid[i] < 0x10) Serial.print("0");
        Serial.print(uid[i], HEX);
        if (i < uidLength - 1) Serial.print(":");
      }
      Serial.println();
      uint8_t data[16];
      if (NFCManager::readBlock(4, data, uid, uidLength)) {
        Serial.print("[NFC] Bloc 4: ");
        for (int i = 0; i < 16; i++) {
          if (data[i] < 0x10) Serial.print("0");
          Serial.print(data[i], HEX);
          Serial.print(" ");
        }
        Serial.println();
        Serial.print("[NFC] Texte: ");
        for (int i = 0; i < 16; i++) {
          Serial.print(data[i] >= 32 && data[i] < 127 ? (char)data[i] : '.');
        }
        Serial.println();
      } else {
        Serial.println("[NFC] Erreur lecture bloc 4");
      }
    } else {
      Serial.println("[NFC] Aucun tag detecte (timeout 10s)");
    }
    if (wasAuto) NFCManager::setAutoDetect(true);
    return true;
  }
  if (command.startsWith("nfc write ")) {
    if (!NFCManager::isAvailable()) {
      Serial.println("[NFC] Module non disponible");
      return true;
    }
    String arg = command.substring(10);
    arg.trim();
    int variantCode = 0;
    if (arg == "bottle" || arg == "1")       variantCode = 1;
    else if (arg == "cake" || arg == "2")    variantCode = 2;
    else if (arg == "apple" || arg == "3")   variantCode = 3;
    else if (arg == "candy" || arg == "4")   variantCode = 4;
    else if (arg == "thermo" || arg == "5")  variantCode = 5;
    else if (arg == "medic" || arg == "6")   variantCode = 6;
    else if (arg == "clean" || arg == "7")   variantCode = 7;
    else if (arg == "play" || arg == "8")    variantCode = 8;
    else if (arg == "sleep" || arg == "9")   variantCode = 9;
    else if (arg == "book" || arg == "10")   variantCode = 10;
    // Pause auto-detect (libère le PN532 du mode détection passive)
    bool wasAutoDetect = NFCManager::isAutoDetectEnabled();
    if (wasAutoDetect) NFCManager::setAutoDetect(false);
    delay(100);  // Laisser le thread NFC terminer son cycle
    NFCManager::writeTag(arg, variantCode);
    // Reprendre auto-detect
    if (wasAutoDetect) NFCManager::setAutoDetect(true);
    return true;
  }
  if (command == "nfc auto on") {
    NFCManager::setAutoDetect(true);
    Serial.println("[NFC] Scan auto active (attention: peut ralentir le touch)");
    return true;
  }
  if (command == "nfc auto off") {
    NFCManager::setAutoDetect(false);
    Serial.println("[NFC] Scan auto desactive");
    return true;
  }
  if (command == "nfc status") {
    Serial.printf("[NFC] Disponible: %s\n", NFCManager::isAvailable() ? "oui" : "non");
    Serial.printf("[NFC] Auto-detect: %s\n", NFCManager::isAutoDetectEnabled() ? "actif" : "inactif");
    Serial.printf("[NFC] Tag present: %s\n", NFCManager::isTagPresent() ? "oui" : "non");
    if (NFCManager::isTagPresent()) {
      uint8_t uid[10];
      uint8_t uidLength;
      if (NFCManager::getLastTagUID(uid, &uidLength)) {
        Serial.print("[NFC] Dernier UID: ");
        for (uint8_t i = 0; i < uidLength; i++) {
          if (uid[i] < 0x10) Serial.print("0");
          Serial.print(uid[i], HEX);
          if (i < uidLength - 1) Serial.print(":");
        }
        Serial.println();
      }
    }
    Serial.printf("[NFC] Firmware: 0x%08X\n", NFCManager::getFirmwareVersion());
    return true;
  }

  // === Speaker ===
  if (command == "speaker" || command == "speaker test") {
    GotchiSpeakerTest::playTone();
    return true;
  }
  if (command == "speaker melody") {
    GotchiSpeakerTest::playMelody();
    return true;
  }
  if (command == "speaker scan") {
    GotchiSpeakerTest::scanES8311();
    return true;
  }
  if (command == "speaker dump") {
    GotchiSpeakerTest::dumpRegisters();
    return true;
  }
  if (command == "speaker sneeze") {
    GotchiSpeakerTest::playSound(SNEEZE_PCM, SNEEZE_PCM_LEN);
    return true;
  }
  if (command.startsWith("speaker vol ")) {
    int vol = command.substring(12).toInt();
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    SDConfig cfg = SDManager::getConfig();
    cfg.speaker_volume = (uint8_t)vol;
    SDManager::saveConfig(cfg);
    Serial.printf("[SPEAKER] Volume sauvé: %d%%\n", vol);
    // Test avec un tone pour entendre le changement
    GotchiSpeakerTest::playTone(440, 300, 70);
    return true;
  }
  if (command == "speaker vol") {
    SDConfig cfg = SDManager::getConfig();
    Serial.printf("[SPEAKER] Volume actuel: %d%%\n", cfg.speaker_volume);
    return true;
  }
  if (command.startsWith("speaker tone ")) {
    // speaker tone <freq> [duration] [volume]
    String args = command.substring(13); args.trim();
    uint16_t freq = 440, dur = 500;
    uint8_t vol = 100;
    int sp1 = args.indexOf(' ');
    if (sp1 < 0) {
      freq = args.toInt();
    } else {
      freq = args.substring(0, sp1).toInt();
      String rest = args.substring(sp1 + 1); rest.trim();
      int sp2 = rest.indexOf(' ');
      if (sp2 < 0) {
        dur = rest.toInt();
      } else {
        dur = rest.substring(0, sp2).toInt();
        vol = rest.substring(sp2 + 1).toInt();
      }
    }
    if (freq < 20) freq = 20;
    if (freq > 8000) freq = 8000;
    if (dur < 50) dur = 50;
    if (dur > 5000) dur = 5000;
    if (vol > 100) vol = 100;
    GotchiSpeakerTest::playTone(freq, dur, vol);
    return true;
  }
  if (command.startsWith("speaker vol ")) {
    uint8_t vol = command.substring(12).toInt();
    if (vol > 100) vol = 100;
    // Joue un tone de test au volume demandé
    GotchiSpeakerTest::playTone(440, 500, vol);
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
  Serial.println("  face thermometer / off              Prendre la temperature");
  Serial.println("  face medicine                Soigner");
  Serial.println("  face brush / off              Brosser les dents");
  Serial.println("  face dirty                   Salir l'ecran (debug)");
  Serial.println("  face wash / off              Activer le frottement pour nettoyer");
  Serial.println("  face poop                    Ajouter un caca (debug)");
  Serial.println("  face clean                   Nettoyer tout");
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
  Serial.println("  face idle <1-25>             Test scene idle :");
  Serial.println("                                1=Daydream  2=LookAround 3=EyeRoll");
  Serial.println("                                4=DoubleTake 5=KnockKnock 6=Hum");
  Serial.println("                                7=CatchStar 8=ChaseFly 9=PeekABoo 10=Wishful");
  Serial.println("                                11=Stretch 12=Sneeze 13=Dance");
  Serial.println("                                14=Hiccup 15=NapAttempt 16=Yawn");
  Serial.println("                                17=Whistle 18=Purr 19=StomachGrowl 20=Reading");
  Serial.println("                                21=Juggle 22=Bubbles 23=Airplane");
  Serial.println("                                24=Grimace 25=PeekABoo2");
  Serial.println("  === NFC ===");
  Serial.println("  nfc scan                     Scanner un tag (UID)");
  Serial.println("  nfc read                     Lire le bloc 4 d'un tag");
  Serial.println("  nfc write <variant>           Ecrire sur un tag");
  Serial.println("                               Food: bottle, cake, apple, candy");
  Serial.println("                               Actions: thermo, medic, clean, play, sleep, book");
  Serial.println("  nfc status                   Etat du module NFC");
  Serial.println("  === Face ===");
  Serial.println("  face <expression>            Force expression");
  Serial.println("  face look <x> <y>            Regard (-1 à 1)");
  Serial.println("  face blink                   Clignement");
}
