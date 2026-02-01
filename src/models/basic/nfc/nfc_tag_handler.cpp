#include "nfc_tag_handler.h"
#include "../../common/managers/nfc/nfc_manager.h"
#include "../../common/managers/audio/audio_manager.h"
#include "../../common/managers/led/led_manager.h"
#include "../config/config.h"

// Variables statiques
bool NFCTagHandler::initialized = false;
bool NFCTagHandler::musicPlaying = false;
uint8_t NFCTagHandler::activeTagUID[10] = {0};
uint8_t NFCTagHandler::activeTagLength = 0;

// ============================================
// Configuration des tags et actions
// ============================================

// Tag pour lancer test.mp3 : F1:B0:0C:01
static const uint8_t TAG_TEST_MUSIC[] = {0xF1, 0xB0, 0x0C, 0x01};
static const uint8_t TAG_TEST_MUSIC_LEN = 4;
static const char* TAG_TEST_MUSIC_FILE = "/test.mp3";

// Couleur des LEDs pour ce tag (bleu)
static const uint8_t TAG_TEST_MUSIC_COLOR_R = 0;
static const uint8_t TAG_TEST_MUSIC_COLOR_G = 0;
static const uint8_t TAG_TEST_MUSIC_COLOR_B = 255;

// ============================================
// Implémentation
// ============================================

void NFCTagHandler::init() {
  if (initialized) return;
  
#if defined(HAS_NFC) && HAS_NFC && defined(HAS_AUDIO) && HAS_AUDIO
  
  // Vérifier que NFC et Audio sont disponibles
  if (!NFCManager::isAvailable()) {
    Serial.println("[NFC-HANDLER] NFC non disponible, handler desactive");
    return;
  }
  
  if (!AudioManager::isAvailable()) {
    Serial.println("[NFC-HANDLER] Audio non disponible, handler desactive");
    return;
  }
  
  // Configurer le callback NFC
  NFCManager::setTagCallback(onTagDetected);
  
  // S'assurer que la détection automatique est activée
  NFCManager::setAutoDetect(true);
  
  initialized = true;
  Serial.println("[NFC-HANDLER] Gestionnaire de tags initialise");
  Serial.println("[NFC-HANDLER] Tag F1:B0:0C:01 -> test.mp3 + LEDs bleues");
  
#else
  Serial.println("[NFC-HANDLER] NFC ou Audio non disponible");
#endif
}

void NFCTagHandler::update() {
#if defined(HAS_NFC) && HAS_NFC && defined(HAS_AUDIO) && HAS_AUDIO
  
  if (!initialized) return;
  
  // Si la musique joue à cause d'un tag, vérifier si le tag est toujours présent
  if (musicPlaying) {
    if (!NFCManager::isTagPresent()) {
      // Le tag a été retiré -> arrêter la musique et les LEDs
      Serial.println("[NFC-HANDLER] Tag retire -> arret musique");
      
      AudioManager::stop();
      
      #if defined(HAS_LED) && HAS_LED
      LEDManager::clear();
      #endif
      
      musicPlaying = false;
      activeTagLength = 0;
    }
  }
  
#endif
}

void NFCTagHandler::onTagDetected(uint8_t* uid, uint8_t uidLength) {
#if defined(HAS_NFC) && HAS_NFC && defined(HAS_AUDIO) && HAS_AUDIO
  
  // Afficher le tag détecté
  Serial.print("[NFC-HANDLER] Tag detecte: ");
  Serial.println(uidToString(uid, uidLength));
  
  // Vérifier si c'est le tag pour test.mp3
  if (matchUID(uid, uidLength, TAG_TEST_MUSIC, TAG_TEST_MUSIC_LEN)) {
    Serial.println("[NFC-HANDLER] Tag reconnu -> lancement test.mp3");
    
    // Sauvegarder l'UID du tag actif
    memcpy(activeTagUID, uid, uidLength);
    activeTagLength = uidLength;
    
    // Lancer la musique
    if (AudioManager::play(TAG_TEST_MUSIC_FILE)) {
      musicPlaying = true;
      
      // Activer les LEDs en bleu avec effet de rotation
      #if defined(HAS_LED) && HAS_LED
      LEDManager::setColor(TAG_TEST_MUSIC_COLOR_R, TAG_TEST_MUSIC_COLOR_G, TAG_TEST_MUSIC_COLOR_B);
      LEDManager::setEffect(LED_EFFECT_ROTATE);
      Serial.println("[NFC-HANDLER] LEDs en bleu avec rotation");
      #endif
      
    } else {
      Serial.println("[NFC-HANDLER] ERREUR: Impossible de lancer la musique");
    }
    
  } else {
    // Tag inconnu
    Serial.println("[NFC-HANDLER] Tag inconnu, aucune action");
  }
  
#endif
}

bool NFCTagHandler::matchUID(uint8_t* uid, uint8_t uidLength, const uint8_t* targetUID, uint8_t targetLength) {
  if (uidLength != targetLength) {
    return false;
  }
  
  for (uint8_t i = 0; i < uidLength; i++) {
    if (uid[i] != targetUID[i]) {
      return false;
    }
  }
  
  return true;
}

String NFCTagHandler::uidToString(uint8_t* uid, uint8_t uidLength) {
  String result = "";
  
  for (uint8_t i = 0; i < uidLength; i++) {
    if (uid[i] < 0x10) result += "0";
    result += String(uid[i], HEX);
    if (i < uidLength - 1) result += ":";
  }
  
  result.toUpperCase();
  return result;
}
