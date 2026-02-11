#include "init_model.h"
#include "../../model_config.h"
#include "../../common/managers/init/init_manager.h"
#ifdef HAS_LCD
#include "../../common/managers/lcd/lcd_manager.h"
#endif
#ifdef HAS_LED
#include "../../common/managers/led/led_manager.h"
#endif
#ifdef HAS_LCD
#include "../managers/emotions/emotion_manager.h"
#include "../managers/emotions/trigger_manager.h"
#endif
#include "../managers/life/life_manager.h"
#ifdef HAS_NFC
#include "../../common/managers/nfc/nfc_manager.h"
#include "../managers/nfc/gotchi_nfc_handler.h"
#include "../config/constants.h"
#endif

/**
 * Initialisation spécifique au modèle Kidoo Gotchi
 */

#ifdef HAS_NFC
/**
 * Callback appelé quand un tag NFC est détecté
 * Lit le bloc 4 du tag, mappe la clé vers une action, et applique l'effet
 */
static void onNFCTagDetected(uint8_t* uid, uint8_t uidLength) {
  Serial.println("[INIT-GOTCHI] Tag NFC detecte par callback!");

  // Afficher l'UID pour debug
  Serial.print("[INIT-GOTCHI] UID: ");
  for (uint8_t i = 0; i < uidLength; i++) {
    if (uid[i] < 0x10) Serial.print("0");
    Serial.print(uid[i], HEX);
    if (i < uidLength - 1) Serial.print(":");
  }
  Serial.println();

  // Lire le bloc 4 pour obtenir la clé
  uint8_t data[16];
  if (!NFCManager::readBlock(4, data, uid, uidLength)) {
    Serial.println("[INIT-GOTCHI] Erreur: Impossible de lire le bloc 4");
    return;
  }

  // Convertir les données en String (la clé est stockée en texte)
  String key = "";
  for (int i = 0; i < 16 && data[i] != 0; i++) {
    key += (char)data[i];
  }

  Serial.printf("[INIT-GOTCHI] Cle lue: '%s'\n", key.c_str());

  // Mapper la clé vers une action
  const NFCKeyMapping* mapping = nullptr;
  for (size_t i = 0; i < NFC_KEY_TABLE_SIZE; i++) {
    if (key == NFC_KEY_TABLE[i].key) {
      mapping = &NFC_KEY_TABLE[i];
      break;
    }
  }

  if (mapping == nullptr) {
    Serial.printf("[INIT-GOTCHI] Cle inconnue: '%s'\n", key.c_str());
    return;
  }

  Serial.printf("[INIT-GOTCHI] Action reconnue: %s (%s)\n", mapping->itemId, mapping->name);

  // Appliquer l'action via LifeManager
  if (LifeManager::applyAction(mapping->itemId)) {
    Serial.printf("[INIT-GOTCHI] Action '%s' appliquee avec succes!\n", mapping->name);

    // Afficher les stats
    GotchiStats stats = LifeManager::getStats();
    Serial.println("[INIT-GOTCHI] Stats actuelles:");
    Serial.printf("[INIT-GOTCHI]   Hunger:    %3d/100\n", stats.hunger);
    Serial.printf("[INIT-GOTCHI]   Happiness: %3d/100\n", stats.happiness);
    Serial.printf("[INIT-GOTCHI]   Health:    %3d/100\n", stats.health);
  } else {
    Serial.printf("[INIT-GOTCHI] Action '%s' en cooldown ou indisponible\n", mapping->name);

    // Afficher le temps restant
    unsigned long remaining = LifeManager::getRemainingCooldown(mapping->itemId);
    if (remaining > 0) {
      unsigned long seconds = remaining / 1000;
      unsigned long minutes = seconds / 60;
      unsigned long hours = minutes / 60;
      Serial.printf("[INIT-GOTCHI] Disponible dans: %luh %lumin\n", hours, minutes % 60);
    }
  }
}
#endif

bool InitModelGotchi::configure() {
  Serial.println("[INIT] Configuration modele Gotchi");
  return true;
}

bool InitModelGotchi::init() {
  Serial.println("");
  Serial.println("========================================");
  Serial.println("[INIT-GOTCHI] Initialisation modele Gotchi");
  Serial.println("========================================");

#ifdef HAS_LCD
  if (LCDManager::isAvailable()) {
    LCDManager::fillScreen(LCDManager::COLOR_BLACK);
    LCDManager::setTextColor(LCDManager::COLOR_GREEN);
    LCDManager::setTextSize(3);
    LCDManager::setCursor(40, 100);
    LCDManager::println("Kidoo");
    LCDManager::setCursor(50, 140);
    LCDManager::println("Gotchi");
    LCDManager::setTextSize(1);
    LCDManager::setTextColor(LCDManager::COLOR_WHITE);
    LCDManager::setCursor(30, 200);
    LCDManager::println("Demarrage...");
    delay(1500);

    // Initialiser le gestionnaire d'émotions
    if (!EmotionManager::init()) {
      Serial.println("[INIT-GOTCHI] Erreur: Impossible d'initialiser EmotionManager");
      Serial.println("[INIT-GOTCHI] Verifiez que config.json existe sur la SD");
    }

    // Initialiser le gestionnaire de triggers automatiques
    if (!TriggerManager::init()) {
      Serial.println("[INIT-GOTCHI] Erreur: Impossible d'initialiser TriggerManager");
      Serial.println("[INIT-GOTCHI] Les triggers automatiques seront desactives");
    }
  }
#endif

  // Initialiser le gestionnaire de vie (Tamagotchi)
  if (!LifeManager::init()) {
    Serial.println("[INIT-GOTCHI] Erreur: Impossible d'initialiser LifeManager");
  }

  // Initialiser le gestionnaire NFC
#ifdef HAS_NFC
  if (!NFCManager::init()) {
    Serial.println("[INIT-GOTCHI] Avertissement: Module NFC non detecte");
    Serial.println("[INIT-GOTCHI] Les commandes NFC seront desactivees");
  } else {
    // Initialiser le gestionnaire NFC Gotchi avec système de variants
    GotchiNFCHandler::init();
    Serial.println("[INIT-GOTCHI] GotchiNFCHandler initialise - systeme de variants actif");
  }
#endif

  // Gotchi : toujours allumer la LED au boot (même en mode BLE / sans WiFi)
  // pour confirmer que la carte et la LED intégrée (GPIO 48) fonctionnent
#ifdef HAS_LED
  if (HAS_LED && LEDManager::isInitialized()) {
    LEDManager::setColor(0, 255, 0);  // vert (évite conflit avec macro COLOR_* de colors.h)
    LEDManager::setEffect(LED_EFFECT_NONE);
  }
#endif

  return true;
}
