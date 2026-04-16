#include "init_model.h"

#include "../lvgl/gotchi_lvgl.h"
#include "../face/behavior/behavior_engine.h"
#include "common/managers/nfc/nfc_manager.h"

// Dernier variant détecté (pour gérer le retrait selon le type)
static uint8_t s_lastVariant = 0;

// Callback NFC : pause le scan, lit le bloc 4 sans contention, reprend le scan
static void onNFCTag(uint8_t* uid, uint8_t uidLength, uint8_t* blockData, bool blockValid) {
  // Pause du scan pour éviter la contention I2C pendant readBlock
  NFCManager::setAutoDetect(false);
  vTaskDelay(pdMS_TO_TICKS(50));  // Laisser le scan en cours finir

  uint8_t data[16] = {0};
  bool readOk = NFCManager::readBlock(4, data, uid, uidLength);

  // Reprendre le scan immédiatement
  NFCManager::setAutoDetect(true);

  if (!readOk) {
    Serial.println("[NFC] Erreur lecture bloc 4 du tag");
    return;
  }

  uint8_t variant = data[0];
  s_lastVariant = variant;

  switch (variant) {
    // --- Nourriture (1-4) ---
    case 1: Serial.println("[NFC] Tag → feed bottle");     BehaviorEngine::feed("bottle"); break;
    case 2: Serial.println("[NFC] Tag → feed cake");       BehaviorEngine::feed("cake");   break;
    case 3: Serial.println("[NFC] Tag → feed apple");      BehaviorEngine::feed("apple");  break;
    case 4: Serial.println("[NFC] Tag → feed candy");      BehaviorEngine::feed("candy");  break;
    // --- Actions (5-8) ---
    case 5: Serial.println("[NFC] Tag → thermometre");     BehaviorEngine::startThermometer(); break;
    case 6: Serial.println("[NFC] Tag → medicament");      BehaviorEngine::giveMedicine();     break;
    case 7: Serial.println("[NFC] Tag → nettoyage");       BehaviorEngine::clean();            break;
    case 8: Serial.println("[NFC] Tag → jeu");             BehaviorEngine::tryPlay();          break;
    case 9:  Serial.println("[NFC] Tag → dodo");            BehaviorEngine::sleep();            break;
    case 10: Serial.println("[NFC] Tag → livre");           BehaviorEngine::readBook();          break;
    default:
      Serial.printf("[NFC] Variant inconnu: %d\n", variant);
      s_lastVariant = 0;
      return;
  }
}

bool InitModelGotchi::init() {
  return GotchiLvgl::init();
}

bool InitModelGotchi::configure() {
  return true;
}

void InitModelGotchi::update() {
  GotchiLvgl::update();

  // Lazy init du callback NFC (le NFC s'initialise après configure())
  static bool nfcCallbackSet = false;
  if (!nfcCallbackSet && NFCManager::isAvailable()) {
    NFCManager::setTagCallback(onNFCTag);
    Serial.println("[GOTCHI] NFC callback configure (feed par tag)");
    nfcCallbackSet = true;
  }

  // Traiter les événements NFC en attente (exécute le callback dans le contexte principal)
  NFCManager::processTagEvents();

  // Stopper l'action en cours quand le tag NFC est retiré
  static bool wasTagPresent = false;
  bool tagNow = NFCManager::isTagPresent();
  if (wasTagPresent && !tagNow) {
    switch (s_lastVariant) {
      case 1: // bottle — retrait du biberon
        BehaviorEngine::stopFeeding();
        Serial.println("[NFC] Tag retire → stop feed");
        break;
      case 5: // thermomètre — retrait
        BehaviorEngine::stopThermometer();
        Serial.println("[NFC] Tag retire → stop thermometre");
        break;
      default:
        Serial.println("[NFC] Tag retire");
        break;
    }
    s_lastVariant = 0;
  }
  wasTagPresent = tagNow;
}
