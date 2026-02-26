#include "init_model.h"
#include "../../model_config.h"
#include "../../../common/managers/init/init_manager.h"
#ifdef HAS_LCD
#include "../../../common/managers/lcd/lcd_manager.h"
#endif
#ifdef HAS_LED
#include "../../../common/managers/led/led_manager.h"
#endif
#ifdef HAS_LCD
#include "../managers/emotions/emotion_manager.h"
#include "../managers/emotions/trigger_manager.h"
#endif
#include "../managers/life/life_manager.h"
#ifdef HAS_NFC
#include "../../../common/managers/nfc/nfc_manager.h"
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

  // Lire le bloc 4 (code variant 1 octet ou clé texte)
  uint8_t data[16];
  if (!NFCManager::readBlock(4, data, uid, uidLength)) {
    Serial.println("[INIT-GOTCHI] Erreur: Impossible de lire le bloc 4");
    return;
  }

  const NFCKeyMapping* mapping = nullptr;
  if (data[0] >= 1 && data[0] <= 4) {
    // Code variant (écrit par gotchi-nfc-write)
    for (size_t i = 0; i < NFC_KEY_TABLE_SIZE; i++) {
      if (NFC_KEY_TABLE[i].variant == data[0]) {
        mapping = &NFC_KEY_TABLE[i];
        Serial.printf("[INIT-GOTCHI] Code lu: %d -> %s\n", data[0], mapping->key);
        break;
      }
    }
  }
  if (mapping == nullptr) {
    // Fallback : clé en texte
    String key = "";
    for (int i = 0; i < 16 && data[i] != 0; i++) {
      key += (char)data[i];
    }
    Serial.printf("[INIT-GOTCHI] Cle lue: '%s'\n", key.c_str());
    for (size_t i = 0; i < NFC_KEY_TABLE_SIZE; i++) {
      if (key == NFC_KEY_TABLE[i].key) {
        mapping = &NFC_KEY_TABLE[i];
        break;
      }
    }
  }

  if (mapping == nullptr) {
    Serial.println("[INIT-GOTCHI] Cle/code inconnu");
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

#ifdef HAS_LCD
void InitModelGotchi::showStartupScreen() {
  LCDManager::setRotation(2);   // Portrait à l'envers : texte pivoté 90° dans l'autre sens
  LCDManager::fillScreen(LCDManager::COLOR_BLACK);
  LCDManager::setTextColor(LCDManager::COLOR_GREEN);
  LCDManager::setTextSize(3);
  LCDManager::setCursor(70, 50);
  LCDManager::println("Kidoo");
  LCDManager::setCursor(80, 95);
  LCDManager::println("Gotchi");
  LCDManager::setTextSize(2);
  LCDManager::setTextColor(LCDManager::COLOR_WHITE);
  LCDManager::setCursor(55, 155);
  LCDManager::println("Demarrage...");
  LCDManager::setRotation(1);   // Restaurer paysage pour le reste de l'app
}
#else
void InitModelGotchi::showStartupScreen() { }
#endif

bool InitModelGotchi::init() {
  Serial.println("");
  Serial.println("========================================");
  Serial.println("[INIT-GOTCHI] Initialisation modele Gotchi");
  Serial.println("========================================");

#ifdef HAS_LCD
  if (LCDManager::isAvailable()) {
    LCDManager::setBacklight(true);
    InitModelGotchi::showStartupScreen();
    delay(1500);

    // Callback pour ré-afficher le nom après la re-init LCD différée (reboot)
    LCDManager::setPostReinitCallback(InitModelGotchi::showStartupScreen);

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
