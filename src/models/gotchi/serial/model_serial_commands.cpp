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
static bool cmdEmotionLoad(const String& args) {
  if (args.length() == 0) {
    Serial.println("[GOTCHI] Usage: emotion-load <key>");
    Serial.println("[GOTCHI] Exemple: emotion-load OK");
    Serial.println("[GOTCHI] Cles disponibles: OK, SLEEP, COLD, etc.");
    return true;
  }

  String key = args;
  key.trim();
  key.toUpperCase();

  Serial.printf("[GOTCHI] Chargement de l'emotion '%s'...\n", key.c_str());

  if (EmotionManager::loadEmotion(key)) {
    Serial.printf("[GOTCHI] Emotion '%s' chargee avec succes!\n", key.c_str());

    const EmotionData* emotion = EmotionManager::getCurrentEmotion();
    if (emotion) {
      Serial.printf("[GOTCHI]   FPS: %d\n", emotion->fps);
      Serial.printf("[GOTCHI]   Taille: %dx%d\n", emotion->width, emotion->height);
      Serial.printf("[GOTCHI]   Total frames: %d\n", emotion->totalFrames);
      Serial.printf("[GOTCHI]   Duree: %.2f s\n", emotion->durationS);
      Serial.printf("[GOTCHI]   Intro: %d frames\n", emotion->intro.frames);
      Serial.printf("[GOTCHI]   Loop: %d frames\n", emotion->loop.frames);
      Serial.printf("[GOTCHI]   Exit: %d frames\n", emotion->exit.frames);
    }
    return true;
  } else {
    Serial.printf("[GOTCHI] Erreur: Impossible de charger l'emotion '%s'\n", key.c_str());
    return true;
  }
}

static bool cmdEmotionIntro(const String& args) {
  if (!EmotionManager::isLoaded()) {
    Serial.println("[GOTCHI] Erreur: Aucune emotion chargee");
    Serial.println("[GOTCHI] Utilisez 'emotion-load <key>' d'abord");
    return true;
  }

  Serial.println("[GOTCHI] Lecture de la phase intro...");
  EmotionManager::playIntro();
  Serial.println("[GOTCHI] Phase intro terminee");
  return true;
}

static bool cmdEmotionLoop(const String& args) {
  if (!EmotionManager::isLoaded()) {
    Serial.println("[GOTCHI] Erreur: Aucune emotion chargee");
    Serial.println("[GOTCHI] Utilisez 'emotion-load <key>' d'abord");
    return true;
  }

  Serial.println("[GOTCHI] Lecture de la phase loop...");
  EmotionManager::playLoop();
  Serial.println("[GOTCHI] Phase loop terminee");
  return true;
}

static bool cmdEmotionExit(const String& args) {
  if (!EmotionManager::isLoaded()) {
    Serial.println("[GOTCHI] Erreur: Aucune emotion chargee");
    Serial.println("[GOTCHI] Utilisez 'emotion-load <key>' d'abord");
    return true;
  }

  Serial.println("[GOTCHI] Lecture de la phase exit...");
  EmotionManager::playExit();
  Serial.println("[GOTCHI] Phase exit terminee");
  return true;
}

static bool cmdEmotionPlay(const String& args) {
  if (!EmotionManager::isLoaded()) {
    Serial.println("[GOTCHI] Erreur: Aucune emotion chargee");
    Serial.println("[GOTCHI] Utilisez 'emotion-load <key>' d'abord");
    return true;
  }

  // Parser le nombre de loops (défaut: 1)
  int loopCount = 1;
  if (args.length() > 0) {
    loopCount = args.toInt();
    if (loopCount < 1) {
      Serial.println("[GOTCHI] Erreur: Le nombre de loops doit etre >= 1");
      return true;
    }
  }

  Serial.printf("[GOTCHI] Lecture de l'emotion complete (intro -> loop x%d -> exit)...\n", loopCount);
  EmotionManager::playAll(loopCount);
  Serial.println("[GOTCHI] Emotion complete terminee");
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
  if (cmd == "emotion-load") {
    return cmdEmotionLoad(args);
  } else if (cmd == "emotion-intro") {
    return cmdEmotionIntro(args);
  } else if (cmd == "emotion-loop") {
    return cmdEmotionLoop(args);
  } else if (cmd == "emotion-exit") {
    return cmdEmotionExit(args);
  } else if (cmd == "emotion-play" || cmd == "emotion-all") {
    return cmdEmotionPlay(args);
  }
#endif

  return false;
}

void ModelGotchiSerialCommands::printHelp() {
  Serial.println("");
  Serial.println("========================================");
  Serial.println("  COMMANDES SPECIFIQUES GOTCHI");
  Serial.println("========================================");
  Serial.println("  gotchi-info      - Afficher les infos du modele Gotchi");
#ifdef HAS_LCD
  Serial.println("");
  Serial.println("--- Commandes Emotions ---");
  Serial.println("  emotion-load <key>     - Charger une emotion (ex: emotion-load OK)");
  Serial.println("  emotion-intro          - Jouer la phase intro de l'emotion");
  Serial.println("  emotion-loop           - Jouer la phase loop de l'emotion");
  Serial.println("  emotion-exit           - Jouer la phase exit de l'emotion");
  Serial.println("  emotion-play [loops]   - Jouer l'emotion complete (intro->loop x N->exit)");
#endif
  Serial.println("========================================");
  Serial.println("");
}
