#include "model_serial_commands.h"
#include <Arduino.h>
#include <Wire.h>
#include "../../model_config.h"
#ifdef HAS_LCD
#include "../managers/emotions/emotion_manager.h"
#endif
#include "../managers/life/life_manager.h"
#include "../config/constants.h"
#ifdef HAS_NFC
#include "../../common/managers/nfc/nfc_manager.h"
#endif
#if defined(HAS_SD) && defined(HAS_WIFI)
#include <SD.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "../../common/managers/wifi/wifi_manager.h"
#include "../../common/managers/sd/sd_manager.h"
#include "../../common/managers/download/download_manager.h"
#include "../../common/config/default_config.h"
#endif

/**
 * Commandes Serial spécifiques au modèle Kidoo Gotchi
 */

// Commande pour scanner le bus I2C (RTC + NFC)
static bool cmdI2cScan(const String& args) {
  Serial.println("[I2C] ========================================");
  Serial.println("[I2C] Scan du bus I2C...");
  Serial.println("[I2C] ========================================");
  Serial.printf("[I2C] SDA Pin: %d\n", NFC_SDA_PIN);
  Serial.printf("[I2C] SCL Pin: %d\n", NFC_SCL_PIN);
  Serial.println("[I2C] ========================================");

  // Initialiser le bus I2C
  Wire.begin(NFC_SDA_PIN, NFC_SCL_PIN);
  Wire.setTimeout(500);
  delay(100);

  int devicesFound = 0;

  Serial.println("[I2C] Scanning addresses 0x01 to 0x7F...");
  Serial.println("");

  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();

    if (error == 0) {
      Serial.printf("[I2C] Device found at 0x%02X", address);

      // Identifier les périphériques courants
      if (address == 0x24) {
        Serial.print(" (PN532 NFC - I2C mode)");
      } else if (address == 0x48) {
        Serial.print(" (PN532 NFC - alternate address)");
      } else if (address == 0x68) {
        Serial.print(" (DS3231 RTC)");
      }

      Serial.println();
      devicesFound++;
    } else if (error == 4) {
      Serial.printf("[I2C] Error at 0x%02X (unknown error)\n", address);
    }

    delay(10);
  }

  Serial.println("");
  Serial.println("[I2C] ========================================");
  if (devicesFound == 0) {
    Serial.println("[I2C] No I2C devices found!");
    Serial.println("[I2C] Check:");
    Serial.println("[I2C]   - Physical connections (SDA/SCL)");
    Serial.println("[I2C]   - Power supply (3.3V/GND)");
    Serial.println("[I2C]   - Pull-up resistors (usually on module)");
  } else {
    Serial.printf("[I2C] Total devices found: %d\n", devicesFound);
  }
  Serial.println("[I2C] ========================================");

  return true;
}

#ifdef HAS_LCD
static bool cmdEmotionLoad(const String& args) {
  if (args.length() == 0) {
    Serial.println("[GOTCHI] Usage: emotion-load <key>");
    Serial.println("[GOTCHI] Exemple: emotion-load OK");
    Serial.println("[GOTCHI] Cles disponibles: OK, SLEEP, COLD, etc.");
    Serial.println("[GOTCHI] Note: Ne charge que les metadonnees (ne joue pas l'emotion)");
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

static bool cmdEmotionPlay(const String& args) {
  // Parser les arguments: [key] [loops]
  // emotion-play OK 3     -> charge et joue OK avec 3 loops
  // emotion-play 3        -> joue l'émotion chargée avec 3 loops
  // emotion-play          -> joue l'émotion chargée avec 1 loop

  String key = "";
  int loopCount = 1;

  if (args.length() > 0) {
    // Parser les arguments
    int spaceIndex = args.indexOf(' ');
    if (spaceIndex > 0) {
      // Deux arguments: key et loops
      key = args.substring(0, spaceIndex);
      String loopsStr = args.substring(spaceIndex + 1);
      loopsStr.trim();
      loopCount = loopsStr.toInt();
      if (loopCount < 0) loopCount = 1;
    } else {
      // Un seul argument: soit key soit loops
      String arg = args;
      arg.trim();

      // Si c'est un nombre, c'est loops
      if (arg.toInt() > 0 || arg == "0") {
        loopCount = arg.toInt();
        if (loopCount < 0) loopCount = 1;
      } else {
        // Sinon c'est une key
        key = arg;
      }
    }
  }

  // Si une key est fournie, l'utiliser. Sinon, utiliser l'émotion chargée
  if (key.length() > 0) {
    key.toUpperCase();
    Serial.printf("[GOTCHI] Requete animation: '%s' (loops=%d)\n", key.c_str(), loopCount);
    if (EmotionManager::requestEmotion(key, loopCount)) {
      Serial.println("[GOTCHI] Animation mise en queue");
    } else {
      Serial.println("[GOTCHI] Erreur: Queue pleine");
    }
  } else {
    // Utiliser l'émotion actuellement chargée
    if (!EmotionManager::isLoaded()) {
      Serial.println("[GOTCHI] Erreur: Aucune emotion chargee");
      Serial.println("[GOTCHI] Utilisez 'emotion-load <key>' ou 'emotion-play <key>' d'abord");
      return true;
    }

    const EmotionData* emotion = EmotionManager::getCurrentEmotion();
    if (emotion) {
      Serial.printf("[GOTCHI] Requete animation: '%s' (loops=%d)\n", emotion->key.c_str(), loopCount);
      if (EmotionManager::requestEmotion(emotion->key, loopCount)) {
        Serial.println("[GOTCHI] Animation mise en queue");
      } else {
        Serial.println("[GOTCHI] Erreur: Queue pleine");
      }
    }
  }

  return true;
}

static bool cmdEmotionStop(const String& args) {
  EmotionManager::cancelAll();
  Serial.println("[GOTCHI] Toutes les animations annulees");
  return true;
}

static bool cmdEmotionStatus(const String& args) {
  const char* stateNames[] = {"IDLE", "PLAYING_INTRO", "PLAYING_LOOP", "PLAYING_EXIT"};
  EmotionPlayState state = EmotionManager::getState();

  Serial.println("[GOTCHI] ========================================");
  Serial.println("[GOTCHI]        STATUT EMOTIONS");
  Serial.println("[GOTCHI] ========================================");
  Serial.printf("[GOTCHI]   Etat:        %s\n", stateNames[state]);
  Serial.printf("[GOTCHI]   En lecture:  %s\n", EmotionManager::isPlaying() ? "OUI" : "NON");

  if (state != EMOTION_STATE_IDLE) {
    String key = EmotionManager::getCurrentPlayingKey();
    if (key.length() > 0) {
      Serial.printf("[GOTCHI]   Emotion:     %s\n", key.c_str());
    }
  }

  Serial.println("[GOTCHI] ========================================");
  return true;
}
#endif

// ============================================
// Commandes du système de vie (Tamagotchi)
// ============================================

static bool cmdGotchiFeed(const String& args) {
  String type = args;
  type.trim();
  type.toLowerCase();

  // Sans type ou "any" = premier aliment disponible (pour le web : donner n'importe quoi à manger)
  const bool anyFood = (type.length() == 0 || type == "any");

  if (!anyFood &&
      type != "bottle" && type != "snack" && type != "cake" && type != "candy" && type != "apple") {
    Serial.println("[GOTCHI] Usage: gotchi-feed [type]");
    Serial.println("[GOTCHI] Sans type ou 'any' = premier aliment disponible.");
    Serial.println("[GOTCHI] Types: bottle, snack, cake, candy, apple");
    return true;
  }

  if (anyFood) {
    Serial.println("[GOTCHI] Nourriture (n'importe lequel)...");
    if (LifeManager::applyFirstAvailableFood()) {
      GotchiStats stats = LifeManager::getStats();
      Serial.println("[GOTCHI] Action appliquee avec succes!");
      Serial.printf("[GOTCHI]   Hunger: %3d/100\n", stats.hunger);
    } else {
      Serial.println("[GOTCHI] Tous les aliments sont en cooldown.");
    }
    return true;
  }

  Serial.printf("[GOTCHI] Simulation de nourriture: %s\n", type.c_str());

  // Appliquer l'action
  if (LifeManager::applyAction(type)) {
    // Afficher les stats après l'action
    GotchiStats stats = LifeManager::getStats();
    Serial.println("[GOTCHI] Action appliquee avec succes!");
    Serial.println("[GOTCHI] Stats actuelles:");
    Serial.printf("[GOTCHI]   Hunger:     %3d/100\n", stats.hunger);
    Serial.printf("[GOTCHI]   Happiness:  %3d/100\n", stats.happiness);
    Serial.printf("[GOTCHI]   Health:    %3d/100\n", stats.health);
    Serial.printf("[GOTCHI]   Fatigue:  %3d/100\n", stats.fatigue);
    Serial.printf("[GOTCHI]   Hygiene: %3d/100\n", stats.hygiene);

    // Afficher le prochain cooldown
    unsigned long cooldown = LifeManager::getRemainingCooldown(type);
    if (cooldown > 0) {
      unsigned long seconds = cooldown / 1000;
      unsigned long minutes = seconds / 60;
      unsigned long hours = minutes / 60;
      Serial.printf("[GOTCHI] Prochain %s disponible dans: %luh %lumin\n",
                    type.c_str(), hours, minutes % 60);
    }
  } else {
    // Action en cooldown
    unsigned long cooldown = LifeManager::getRemainingCooldown(type);
    unsigned long seconds = cooldown / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    Serial.printf("[GOTCHI] Erreur: %s en cooldown\n", type.c_str());
    Serial.printf("[GOTCHI] Disponible dans: %luh %lumin %lus\n",
                  hours, minutes % 60, seconds % 60);
  }

  return true;
}

static bool cmdGotchiStatus(const String& args) {
  Serial.println("[GOTCHI] ========================================");
  Serial.println("[GOTCHI]        STATS DU GOTCHI");
  Serial.println("[GOTCHI] ========================================");

  GotchiStats stats = LifeManager::getStats();

  // Afficher les stats avec des barres de progression
  auto printStatBar = [](const char* name, uint8_t value) {
    Serial.printf("[GOTCHI]   %-9s [", name);
    int bars = value / 5; // 20 barres pour 100
    for (int i = 0; i < 20; i++) {
      if (i < bars) {
        Serial.print("=");
      } else {
        Serial.print(" ");
      }
    }
    Serial.printf("] %3d/100\n", value);
  };

  printStatBar("Hunger", stats.hunger);
  printStatBar("Happiness", stats.happiness);
  printStatBar("Health", stats.health);
  printStatBar("Fatigue", stats.fatigue);
  printStatBar("Hygiene", stats.hygiene);

  // Afficher les cooldowns actifs
  Serial.println("[GOTCHI] ========================================");
  Serial.println("[GOTCHI]        COOLDOWNS ACTIFS");
  Serial.println("[GOTCHI] ========================================");

  const char* actions[] = {"bottle", "cake", "candy", "apple"};
  bool anyCooldown = false;

  for (const char* action : actions) {
    unsigned long cooldown = LifeManager::getRemainingCooldown(action);
    if (cooldown > 0) {
      anyCooldown = true;
      unsigned long seconds = cooldown / 1000;
      unsigned long minutes = seconds / 60;
      unsigned long hours = minutes / 60;
      Serial.printf("[GOTCHI]   %-10s %luh %lumin %lus\n",
                    action, hours, minutes % 60, seconds % 60);
    }
  }

  if (!anyCooldown) {
    Serial.println("[GOTCHI]   Aucun cooldown actif");
  }

  Serial.println("[GOTCHI] ========================================");
  return true;
}

static bool cmdGotchiTick(const String& args) {
  Serial.println("[GOTCHI] Simulation d'un cycle de 30 minutes...");
  Serial.println("[GOTCHI] Declin force des stats");

  LifeManager::forceStatDecline();

  // Afficher les nouvelles stats
  GotchiStats stats = LifeManager::getStats();
  Serial.println("[GOTCHI] Stats apres declin:");
  Serial.printf("[GOTCHI]   Hunger:     %3d/100 (-%d)\n", stats.hunger, STATS_HUNGER_DECLINE_RATE);
  Serial.printf("[GOTCHI]   Happiness:  %3d/100\n", stats.happiness);
  Serial.printf("[GOTCHI]   Health:    %3d/100\n", stats.health);
  Serial.printf("[GOTCHI]   Fatigue:  %3d/100\n", stats.fatigue);
  Serial.printf("[GOTCHI]   Hygiene: %3d/100\n", stats.hygiene);

  return true;
}

static bool cmdGotchiReset(const String& args) {
  Serial.println("[GOTCHI] Reinitialisation des stats du Gotchi...");

  LifeManager::resetStats();

  // Afficher les stats réinitialisées
  GotchiStats stats = LifeManager::getStats();
  Serial.println("[GOTCHI] Stats reinitialisees:");
  Serial.printf("[GOTCHI]   Hunger:     %3d/100\n", stats.hunger);
  Serial.printf("[GOTCHI]   Happiness:  %3d/100\n", stats.happiness);
  Serial.printf("[GOTCHI]   Health:    %3d/100\n", stats.health);
  Serial.printf("[GOTCHI]   Fatigue:  %3d/100\n", stats.fatigue);
  Serial.printf("[GOTCHI]   Hygiene: %3d/100\n", stats.hygiene);
  Serial.println("[GOTCHI] Tous les cooldowns ont ete reinitialises");

  return true;
}

static bool cmdGotchiSet(const String& args) {
  if (args.length() == 0) {
    Serial.println("[GOTCHI] Usage: gotchi-set <stat> <delta>");
    Serial.println("[GOTCHI] Available stats: hunger, happiness, health, fatigue, hygiene");
    Serial.println("[GOTCHI] Delta: valeur a ajouter (peut etre negative)");
    Serial.println("[GOTCHI] Exemples:");
    Serial.println("[GOTCHI]   gotchi-set hunger -10      -> Decrease hunger by 10");
    Serial.println("[GOTCHI]   gotchi-set happiness +15 -> Increase happiness by 15");
    Serial.println("[GOTCHI]   gotchi-set health 50      -> Set health to 50 (relative to current value)");
    return true;
  }

  // Parser les arguments (stat et delta)
  int spaceIndex = args.indexOf(' ');
  if (spaceIndex < 0) {
    Serial.println("[GOTCHI] Erreur: Syntaxe invalide");
    Serial.println("[GOTCHI] Usage: gotchi-set <stat> <delta>");
    return true;
  }

  String statName = args.substring(0, spaceIndex);
  String deltaStr = args.substring(spaceIndex + 1);
  statName.trim();
  deltaStr.trim();

  // Convertir le delta en entier
  int delta = deltaStr.toInt();
  if (delta == 0 && deltaStr != "0") {
    Serial.printf("[GOTCHI] Erreur: Delta invalide '%s'\n", deltaStr.c_str());
    return true;
  }

  // Obtenir les stats avant modification
  GotchiStats statsBefore = LifeManager::getStats();

  // Appliquer la modification
  if (LifeManager::adjustStat(statName, delta)) {
    // Afficher les stats après modification
    GotchiStats statsAfter = LifeManager::getStats();
    Serial.println("[GOTCHI] Stats apres modification:");
    Serial.printf("[GOTCHI]   Hunger:    %3d/100\n", statsAfter.hunger);
    Serial.printf("[GOTCHI]   Happiness: %3d/100\n", statsAfter.happiness);
    Serial.printf("[GOTCHI]   Health:    %3d/100\n", statsAfter.health);
    Serial.printf("[GOTCHI]   Fatigue:  %3d/100\n", statsAfter.fatigue);
    Serial.printf("[GOTCHI]   Hygiene:   %3d/100\n", statsAfter.hygiene);
  } else {
    Serial.println("[GOTCHI] Erreur: Impossible de modifier la stat");
    Serial.println("[GOTCHI] Valid stats: hunger, happiness, health, fatigue, hygiene");
  }

  return true;
}

static bool cmdGotchiNfc(const String& args) {
  if (args.length() == 0) {
    Serial.println("[GOTCHI] Usage: gotchi-nfc <key>");
    Serial.println("[GOTCHI] Simule la lecture d'un badge NFC avec la cle specifiee");
    Serial.println("[GOTCHI] Cles disponibles:");
    for (size_t i = 0; i < NFC_KEY_TABLE_SIZE; i++) {
      Serial.printf("[GOTCHI]   %s - %s\n",
                    NFC_KEY_TABLE[i].key,
                    NFC_KEY_TABLE[i].name);
    }
    return true;
  }

  String key = args;
  key.trim();
  key.toUpperCase();

  // Rechercher la clé dans la table
  const NFCKeyMapping* mapping = nullptr;
  for (size_t i = 0; i < NFC_KEY_TABLE_SIZE; i++) {
    String tableKey = String(NFC_KEY_TABLE[i].key);
    tableKey.toUpperCase();
    if (key == tableKey) {
      mapping = &NFC_KEY_TABLE[i];
      break;
    }
  }

  if (mapping == nullptr) {
    Serial.printf("[GOTCHI] Erreur: Cle '%s' inconnue\n", key.c_str());
    Serial.println("[GOTCHI] Utilisez 'gotchi-nfc' sans argument pour voir les cles disponibles");
    return true;
  }

  // Afficher l'objet détecté
  Serial.println("[GOTCHI] ========================================");
  Serial.printf("[GOTCHI] Badge NFC detecte: %s\n", mapping->name);
  Serial.println("[GOTCHI] ========================================");

  // Appliquer l'action correspondante
  String itemId = String(mapping->itemId);
  if (LifeManager::applyAction(itemId)) {
    // Afficher les stats après l'action
    GotchiStats stats = LifeManager::getStats();
    Serial.println("[GOTCHI] Action appliquee avec succes!");
    Serial.println("[GOTCHI] Stats actuelles:");
    Serial.printf("[GOTCHI]   Hunger:    %3d/100\n", stats.hunger);
    Serial.printf("[GOTCHI]   Happiness: %3d/100\n", stats.happiness);
    Serial.printf("[GOTCHI]   Health:    %3d/100\n", stats.health);
    Serial.printf("[GOTCHI]   Fatigue:   %3d/100\n", stats.fatigue);
    Serial.printf("[GOTCHI]   Hygiene:   %3d/100\n", stats.hygiene);

    // Afficher le prochain cooldown
    unsigned long cooldown = LifeManager::getRemainingCooldown(itemId);
    if (cooldown > 0) {
      unsigned long seconds = cooldown / 1000;
      unsigned long minutes = seconds / 60;
      unsigned long hours = minutes / 60;
      Serial.printf("[GOTCHI] Prochain %s disponible dans: %luh %lumin\n",
                    mapping->name, hours, minutes % 60);
    }
  } else {
    // Action en cooldown
    unsigned long cooldown = LifeManager::getRemainingCooldown(itemId);
    unsigned long seconds = cooldown / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    Serial.printf("[GOTCHI] Erreur: %s en cooldown\n", mapping->name);
    Serial.printf("[GOTCHI] Disponible dans: %luh %lumin %lus\n",
                  hours, minutes % 60, seconds % 60);
  }

  Serial.println("[GOTCHI] ========================================");
  return true;
}

static bool cmdGotchiNfcWrite(const String& args) {
#ifdef HAS_NFC
  if (args.length() == 0) {
    Serial.println("[GOTCHI] Usage: gotchi-nfc-write <key>");
    Serial.println("[GOTCHI] Ecrit un code (1 octet) sur le tag pour reconnaissance fiable");
    Serial.println("[GOTCHI] Cles disponibles (ecriture code 1-4):");
    for (size_t i = 0; i < NFC_KEY_TABLE_SIZE; i++) {
      Serial.printf("[GOTCHI]   %s - %s (code %d)\n",
                    NFC_KEY_TABLE[i].key,
                    NFC_KEY_TABLE[i].name,
                    NFC_KEY_TABLE[i].variant);
    }
    return true;
  }

  if (!NFCManager::isAvailable()) {
    Serial.println("[GOTCHI] Erreur: Module NFC non initialise");
    Serial.println("[GOTCHI] Verifiez que le module PN532 est bien connecte");
    return true;
  }

  String key = args;
  key.trim();
  key.toUpperCase();

  // Rechercher la clé dans la table
  const NFCKeyMapping* mapping = nullptr;
  for (size_t i = 0; i < NFC_KEY_TABLE_SIZE; i++) {
    String tableKey = String(NFC_KEY_TABLE[i].key);
    tableKey.toUpperCase();
    if (key == tableKey) {
      mapping = &NFC_KEY_TABLE[i];
      break;
    }
  }

  if (mapping == nullptr) {
    Serial.printf("[GOTCHI] Erreur: Cle '%s' inconnue\n", key.c_str());
    Serial.println("[GOTCHI] Utilisez 'gotchi-nfc-write' sans argument pour voir les cles disponibles");
    return true;
  }

  Serial.println("[GOTCHI] ========================================");
  Serial.printf("[GOTCHI] Ecriture de la cle: %s (%s)\n", mapping->key, mapping->name);
  Serial.println("[GOTCHI] ========================================");

  // Écrire le code variant (1 octet) sur le tag pour reconnaissance fiable
  if (NFCManager::writeTag(String(mapping->key), mapping->variant)) {
    Serial.println("[GOTCHI] ========================================");
    Serial.println("[GOTCHI] Ecriture reussie!");
    Serial.printf("[GOTCHI] Le tag contient le code: %d (%s)\n", mapping->variant, mapping->key);
    Serial.println("[GOTCHI] ========================================");
  } else {
    Serial.println("[GOTCHI] ========================================");
    Serial.println("[GOTCHI] Erreur: Ecriture echouee");
    Serial.println("[GOTCHI] Verifiez que:");
    Serial.println("[GOTCHI]   - Un tag NFC est proche du lecteur");
    Serial.println("[GOTCHI]   - Le tag est compatible MIFARE Classic");
    Serial.println("[GOTCHI]   - Le module PN532 fonctionne correctement");
    Serial.println("[GOTCHI] ========================================");
  }

  return true;
#else
  Serial.println("[GOTCHI] Erreur: NFC non disponible sur ce modele");
  return true;
#endif
}

#if defined(HAS_SD) && defined(HAS_WIFI)
/**
 * sync-emotions
 * Récupère la liste des émotions et la config depuis le serveur (API_BASE_URL).
 * GET /api/kidoos/emotions-sync?characterId=xxx[&since=ISO8601]
 * Le serveur renvoie toujours la config complète ; si since est envoyé, seul le champ "files"
 * contient les medias a telecharger (delta). On ecrit toujours la config reçue (pas de fusion).
 * Après succès, met à jour emotionsSyncLastAt avec syncedAt.
 */
static void syncEmotionsProgress(int current, int total, const char* localPath, bool success) {
  Serial.printf("[SYNC-EMOTIONS] [%d/%d] %s\n", current, total, localPath ? localPath : "");
  Serial.println(success ? "[SYNC-EMOTIONS]   OK" : "[SYNC-EMOTIONS]   Echec");
}

/** Encode une chaîne pour un paramètre de requête (pour since ISO8601). */
static void urlEncodeQueryParam(String& out, const char* s) {
  if (!s) return;
  for (; *s; s++) {
    char c = *s;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
      out += c;
    else if (c == ' ')
      out += F("%20");
    else if (c == ':')
      out += F("%3A");
    else if (c == '+')
      out += F("%2B");
    else if (c == '%')
      out += F("%25");
    else {
      char hex[4];
      snprintf(hex, sizeof(hex), "%%%02X", (unsigned char)c);
      out += hex;
    }
  }
}

static bool cmdSyncEmotions(const String& args) {
  (void)args;

  if (!SDManager::isAvailable()) {
    Serial.println("[SYNC-EMOTIONS] Erreur: SD non disponible");
    return true;
  }

  // Lire characterId depuis /config.json
  if (!SD.exists("/config.json")) {
    Serial.println("[SYNC-EMOTIONS] Erreur: /config.json introuvable sur la SD");
    Serial.println("[SYNC-EMOTIONS] Creez /config.json avec {\"characterId\": \"<uuid>\"}");
    return true;
  }

  File configFile = SD.open("/config.json", FILE_READ);
  if (!configFile) {
    Serial.println("[SYNC-EMOTIONS] Erreur: impossible d'ouvrir /config.json");
    return true;
  }

  JsonDocument configDoc;
  DeserializationError err = deserializeJson(configDoc, configFile);
  configFile.close();
  if (err || configDoc["characterId"].isNull()) {
    Serial.println("[SYNC-EMOTIONS] Erreur: characterId manquant dans /config.json");
    return true;
  }

  String characterId = configDoc["characterId"].as<String>();
  if (characterId.isEmpty()) {
    Serial.println("[SYNC-EMOTIONS] Erreur: characterId vide");
    return true;
  }

  String emotionsSyncLastAt;
  if (!configDoc["emotionsSyncLastAt"].isNull() && configDoc["emotionsSyncLastAt"].is<const char*>()) {
    emotionsSyncLastAt = configDoc["emotionsSyncLastAt"].as<String>();
    emotionsSyncLastAt.trim();
  }

  if (!WiFiManager::isConnected()) {
    Serial.println("[SYNC-EMOTIONS] Erreur: WiFi non connecte. Connectez le WiFi puis reessayez.");
    return true;
  }

  String url = String(API_BASE_URL);
  if (url.endsWith("/")) url.remove(url.length() - 1);
  url += "/api/kidoos/emotions-sync?characterId=";
  url += characterId;
  if (!emotionsSyncLastAt.isEmpty()) {
    url += "&since=";
    urlEncodeQueryParam(url, emotionsSyncLastAt.c_str());
    Serial.printf("[SYNC-EMOTIONS] Sync incrémental depuis %s\n", emotionsSyncLastAt.c_str());
  } else {
    Serial.println("[SYNC-EMOTIONS] Pas de date since: telechargement complet (ajoutez emotionsSyncLastAt dans config.json apres ce sync).");
  }

  Serial.println("[SYNC-EMOTIONS] Recuperation depuis le serveur...");
  Serial.println(url);

  HTTPClient http;
  http.begin(url);
  http.setConnectTimeout(10000);
  http.setTimeout(30000);

  int httpCode = http.GET();

  if (httpCode != 200) {
    Serial.printf("[SYNC-EMOTIONS] Erreur HTTP: %d\n", httpCode);
    http.end();
    return true;
  }

  String payload = http.getString();
  http.end();

  if (payload.length() == 0) {
    Serial.println("[SYNC-EMOTIONS] Reponse vide");
    return true;
  }

  JsonDocument doc;
  err = deserializeJson(doc, payload, DeserializationOption::NestingLimit(24));
  if (err) {
    Serial.printf("[SYNC-EMOTIONS] Erreur JSON: %s\n", err.c_str());
    return true;
  }

  if (!doc["success"].as<bool>() || !doc["data"].is<JsonObject>()) {
    Serial.println("[SYNC-EMOTIONS] Reponse invalide (success/data)");
    return true;
  }

  JsonObject data = doc["data"];
  JsonArray config = data["config"].as<JsonArray>();
  JsonArray files = data["files"].as<JsonArray>();
  const char* syncedAtStr = data["syncedAt"] | "";

  // Créer /characters/<id>/emotions/ si besoin
  String dirPath = "/characters/" + characterId + "/emotions";
  if (!SD.exists("/characters")) SD.mkdir("/characters");
  String charDir = "/characters/" + characterId;
  if (!SD.exists(charDir.c_str())) SD.mkdir(charDir.c_str());
  if (!SD.exists(dirPath.c_str())) SD.mkdir(dirPath.c_str());

  String configPath = dirPath + "/config.json";

  // Toujours écrire la config reçue (complète côté serveur). Pas de fusion, pas de lecture du fichier existant.
  if (config.size() == 0 && SD.exists(configPath.c_str())) {
    Serial.println("[SYNC-EMOTIONS] Aucune donnee config dans la reponse: fichier actuel conserve (pas d'ecrasement par vide).");
  } else if (config.size() > 0) {
    File outFile = SD.open(configPath.c_str(), FILE_WRITE);
    if (!outFile) {
      Serial.printf("[SYNC-EMOTIONS] Erreur: impossible d'ecrire %s\n", configPath.c_str());
      return true;
    }
    size_t written = serializeJson(config, outFile);
    outFile.close();
    Serial.printf("[SYNC-EMOTIONS] Config sauvegardee: %s (%u octets, %u entrees)\n",
                  configPath.c_str(), (unsigned)written, (unsigned)config.size());
  }

  const size_t totalFiles = files.size() * 2;
  if (totalFiles == 0) {
    Serial.println("[SYNC-EMOTIONS] Aucun fichier a telecharger.");
  } else {
    const size_t maxFiles = 128;
    if (totalFiles > maxFiles) {
      Serial.printf("[SYNC-EMOTIONS] Trop de fichiers (%u), limite %u\n", (unsigned)totalFiles, (unsigned)maxFiles);
    } else {
      const char* urls[maxFiles];
      const char* paths[maxFiles];
      size_t idx = 0;
      for (size_t i = 0; i < files.size(); i++) {
        JsonObject f = files[i];
        urls[idx] = f["mjpegUrl"] | "";
        paths[idx] = f["localPathMjpeg"].as<const char*>();
        idx++;
        urls[idx] = f["idxUrl"] | "";
        paths[idx] = f["localPathIdx"].as<const char*>();
        idx++;
      }
      Serial.printf("[SYNC-EMOTIONS] Telechargement de %u fichier(s) medias (mjpeg+idx, connexion reuse par hote)...\n", (unsigned)totalFiles);
      int okCount = DownloadManager::downloadUrlsToFiles(urls, paths, (int)totalFiles, syncEmotionsProgress);
      Serial.printf("[SYNC-EMOTIONS] Termine: %d/%u fichier(s). Redemarrez ou reinit pour prendre en compte.\n",
                    okCount, (unsigned)totalFiles);
    }
  }

  // Mettre à jour la date de dernier sync dans /config.json pour le prochain sync incrémental
  if (syncedAtStr[0] != '\0') {
    File rootConfig = SD.open("/config.json", FILE_READ);
    if (rootConfig) {
      size_t sz = (size_t)rootConfig.size();
      if (sz > 0 && sz <= 4096) {
        JsonDocument rootDoc;
        DeserializationError parseErr = deserializeJson(rootDoc, rootConfig, DeserializationOption::NestingLimit(16));
        rootConfig.close();
        if (!parseErr && !rootDoc.isNull()) {
          rootDoc["emotionsSyncLastAt"] = syncedAtStr;
          rootConfig = SD.open("/config.json", FILE_WRITE);
          if (rootConfig) {
            serializeJson(rootDoc, rootConfig);
            rootConfig.close();
            Serial.printf("[SYNC-EMOTIONS] emotionsSyncLastAt mis a jour: %s (prochain sync: incrémental)\n", syncedAtStr);
          } else {
            Serial.println("[SYNC-EMOTIONS] Impossible d'ecrire config.json pour emotionsSyncLastAt");
          }
        } else {
          Serial.printf("[SYNC-EMOTIONS] Erreur parsing config.json pour emotionsSyncLastAt: %s (ajoutez-le avec config-set)\n", parseErr ? parseErr.c_str() : "doc null");
        }
      } else {
        rootConfig.close();
        if (sz > 4096) {
          Serial.println("[SYNC-EMOTIONS] config.json trop volumineux (>4Ko): emotionsSyncLastAt non mis a jour. Ajoutez-le avec config-set.");
        }
      }
    } else {
      Serial.println("[SYNC-EMOTIONS] Impossible d'ouvrir /config.json pour mettre a jour emotionsSyncLastAt");
    }
  } else {
    Serial.println("[SYNC-EMOTIONS] Serveur n'a pas renvoye syncedAt, emotionsSyncLastAt non mis a jour.");
  }
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

  if (cmd == "i2c-scan") {
    return cmdI2cScan(args);
  }

  // Commandes du système de vie
  if (cmd == "gotchi-feed") {
    return cmdGotchiFeed(args);
  } else if (cmd == "gotchi-status") {
    return cmdGotchiStatus(args);
  } else if (cmd == "gotchi-tick") {
    return cmdGotchiTick(args);
  } else if (cmd == "gotchi-reset") {
    return cmdGotchiReset(args);
  } else if (cmd == "gotchi-set") {
    return cmdGotchiSet(args);
  } else if (cmd == "gotchi-nfc") {
    return cmdGotchiNfc(args);
  } else if (cmd == "gotchi-nfc-write") {
    return cmdGotchiNfcWrite(args);
  }

  if (cmd == "sync-emotions") {
#if defined(HAS_SD) && defined(HAS_WIFI)
    return cmdSyncEmotions(args);
#else
    Serial.println("[SYNC-EMOTIONS] Non disponible: compilation sans HAS_SD ou HAS_WIFI");
    return true;
#endif
  }

#ifdef HAS_LCD
  if (cmd == "emotion-load") {
    return cmdEmotionLoad(args);
  } else if (cmd == "emotion-play" || cmd == "emotion-all") {
    return cmdEmotionPlay(args);
  } else if (cmd == "emotion-stop") {
    return cmdEmotionStop(args);
  } else if (cmd == "emotion-status") {
    return cmdEmotionStatus(args);
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
  Serial.println("  i2c-scan         - Scanner le bus I2C (debug NFC/RTC)");
  Serial.println("");
  Serial.println("--- Commandes Systeme de Vie ---");
  Serial.println("  gotchi-feed [type]    - Nourrir (sans type = 1er dispo; bottle/cake/candy/apple)");
  Serial.println("  gotchi-status          - Afficher les stats et cooldowns");
  Serial.println("  gotchi-tick            - Forcer le declin des stats (cycle 30min)");
  Serial.println("  gotchi-reset           - Reinitialiser toutes les stats");
  Serial.println("  gotchi-set <stat> <delta> - Modifier une stat manuellement");
  Serial.println("  gotchi-nfc <key>       - Simuler la lecture d'un badge NFC");
  Serial.println("  gotchi-nfc-write <key> - Ecrire une cle sur un tag NFC physique");
#if defined(HAS_SD) && defined(HAS_WIFI)
  Serial.println("  sync-emotions           - Recuperer config emotions depuis l'API (API_BASE_URL, sauvegarde SD)");
#endif
#ifdef HAS_LCD
  Serial.println("");
  Serial.println("--- Commandes Emotions (systeme asynchrone) ---");
  Serial.println("  emotion-load <key>        - Charger metadonnees emotion (ex: emotion-load OK)");
  Serial.println("  emotion-play [key] [loops]- Jouer emotion (ex: emotion-play OK 3)");
  Serial.println("  emotion-stop              - Annuler toutes les animations");
  Serial.println("  emotion-status            - Afficher l'etat du systeme d'emotions");
#endif
  Serial.println("========================================");
  Serial.println("");
}
