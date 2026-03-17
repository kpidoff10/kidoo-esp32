#include "model_serial_commands.h"
#include <Arduino.h>

bool ModelSoundSerialCommands::processCommand(const String& command) {
  // Parser la commande : "audio <subcommand> [args]"
  if (!command.startsWith("audio ")) {
    return false;
  }

  String args = command.substring(6);  // Skip "audio "
  args.trim();

  if (args == "play" || args.startsWith("play ")) {
    String path = "/test.mp3";  // Par défaut

    if (args.startsWith("play ")) {
      path = args.substring(5);  // Skip "play "
      path.trim();
      if (!path.isEmpty()) {
        // Chemin spécifié
      } else {
        path = "/test.mp3";
      }
    }

    Serial.printf("[AUDIO CMD] Lecture: %s\n", path.c_str());
    if (AudioManager::play(path.c_str())) {
      Serial.println("[AUDIO CMD] Lecture demarree");
    } else {
      Serial.println("[AUDIO CMD] ERREUR: Impossible de lire le fichier");
    }
    return true;
  }

  else if (args == "stop") {
    AudioManager::stop();
    Serial.println("[AUDIO CMD] Lecture arretee");
    return true;
  }

  else if (args == "pause") {
    AudioManager::pause();
    Serial.println("[AUDIO CMD] Lecture en pause");
    return true;
  }

  else if (args == "resume") {
    AudioManager::resume();
    Serial.println("[AUDIO CMD] Lecture reprise");
    return true;
  }

  else if (args.startsWith("volume ")) {
    String volStr = args.substring(7);  // Skip "volume "
    volStr.trim();
    int vol = volStr.toInt();

    if (vol < 0 || vol > 100) {
      Serial.println("[AUDIO CMD] Volume invalide (0-100)");
    } else {
      AudioManager::setVolume((uint8_t)vol);
      Serial.printf("[AUDIO CMD] Volume: %d%%\n", vol);
    }
    return true;
  }

  else if (args == "status") {
    AudioManager::printStatus();
    return true;
  }

  else if (args == "test") {
    // Commande de test rapide : jouer test.mp3
    Serial.println("[AUDIO CMD] Test: Lecture de /test.mp3");
    if (AudioManager::play("/test.mp3")) {
      Serial.println("[AUDIO CMD] Test lance!");
      AudioManager::setVolume(50);  // 50% volume par défaut
    } else {
      Serial.println("[AUDIO CMD] Test ERREUR");
    }
    return true;
  }

  return false;
}

void ModelSoundSerialCommands::printHelp() {
  Serial.println("");
  Serial.println("========== Commandes Audio Sound ==========");
  Serial.println("  audio play [path]      - Jouer un fichier (default: /test.mp3)");
  Serial.println("  audio stop             - Arreter la lecture");
  Serial.println("  audio pause            - Mettre en pause");
  Serial.println("  audio resume           - Reprendre");
  Serial.println("  audio volume <0-100>   - Definir le volume (%)");
  Serial.println("  audio status           - Afficher le statut audio");
  Serial.println("  audio test             - Test rapide avec /test.mp3");
  Serial.println("=========================================");
}
