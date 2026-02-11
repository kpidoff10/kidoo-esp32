#include "gotchi_nfc_handler.h"
#include "../../../common/managers/nfc/nfc_manager.h"
#include "../emotions/emotion_manager.h"
#include "../emotions/trigger_manager.h"
#include "../life/life_manager.h"
#include "../../../model_config.h"

// Variables statiques
bool GotchiNFCHandler::initialized = false;
bool GotchiNFCHandler::tagPresent = false;
uint8_t GotchiNFCHandler::activeTagUID[10] = {0};
uint8_t GotchiNFCHandler::activeTagLength = 0;

// ============================================
// Configuration des badges NFC (UIDs)
// ============================================

// TODO: Remplacer par les vrais UIDs de vos badges NFC
// Variant 1: Biberon
static const uint8_t BADGE_BOTTLE_UID[] = {0xF1, 0xB0, 0x0C, 0x01};
static const uint8_t BADGE_BOTTLE_LEN = 4;

// Variant 2: Gâteau
static const uint8_t BADGE_CAKE_UID[] = {0xF1, 0xB0, 0x0C, 0x02};
static const uint8_t BADGE_CAKE_LEN = 4;

// Variant 3: Pomme
static const uint8_t BADGE_APPLE_UID[] = {0xF1, 0xB0, 0x0C, 0x03};
static const uint8_t BADGE_APPLE_LEN = 4;

// Variant 4: Bonbon
static const uint8_t BADGE_CANDY_UID[] = {0xF1, 0xB0, 0x0C, 0x04};
static const uint8_t BADGE_CANDY_LEN = 4;

// ============================================
// Implémentation
// ============================================

void GotchiNFCHandler::init() {
  if (initialized) return;

#if defined(HAS_NFC) && HAS_NFC

  // Vérifier que NFC est disponible
  if (!NFCManager::isAvailable()) {
    Serial.println("[GOTCHI-NFC] NFC non disponible");
    return;
  }

  // Vérifier que EmotionManager et TriggerManager sont initialisés
  if (!EmotionManager::isLoaded() && !TriggerManager::isEnabled()) {
    Serial.println("[GOTCHI-NFC] Warning: EmotionManager ou TriggerManager non initialise");
  }

  // Configurer le callback NFC
  NFCManager::setTagCallback(onTagDetected);

  // Activer la détection automatique
  NFCManager::setAutoDetect(true);

  initialized = true;
  Serial.println("[GOTCHI-NFC] Gestionnaire NFC Gotchi initialise");
  Serial.println("[GOTCHI-NFC] Variants: 1=bottle, 2=cake, 3=apple, 4=candy");

#else
  Serial.println("[GOTCHI-NFC] NFC non disponible dans cette configuration");
#endif
}

void GotchiNFCHandler::update() {
#if defined(HAS_NFC) && HAS_NFC

  if (!initialized) return;

  // Si un tag était présent, vérifier s'il a été retiré
  if (tagPresent) {
    if (!NFCManager::isTagPresent()) {
      Serial.println("[GOTCHI-NFC] Tag retire");
      tagPresent = false;
      activeTagLength = 0;
    }
  }

#endif
}

void GotchiNFCHandler::onTagDetected(uint8_t* uid, uint8_t uidLength) {
#if defined(HAS_NFC) && HAS_NFC

  Serial.print("[GOTCHI-NFC] Tag detecte: ");
  Serial.println(uidToString(uid, uidLength));

  // Sauvegarder l'UID du tag actif
  memcpy(activeTagUID, uid, uidLength);
  activeTagLength = uidLength;
  tagPresent = true;

  // Obtenir le variant du badge NFC
  int badgeVariant = getVariantForBadge(uid, uidLength);

  if (badgeVariant == 0) {
    Serial.println("[GOTCHI-NFC] Badge inconnu, aucune action");
    return;
  }

  Serial.printf("[GOTCHI-NFC] Badge variant: %d\n", badgeVariant);

  // Obtenir le variant demandé par le Gotchi (depuis le dernier trigger)
  int requestedVariant = TriggerManager::getRequestedVariant();

  Serial.printf("[GOTCHI-NFC] Variant demande par Gotchi: %d\n", requestedVariant);

  // Si pas de variant demandé (pas de trigger actif), accepter n'importe quel badge
  if (requestedVariant == 0) {
    Serial.println("[GOTCHI-NFC] Aucun variant demande, accepte le badge");
    requestedVariant = badgeVariant;  // Accepter
  }

  // Vérifier si le badge correspond au variant demandé
  if (badgeVariant != requestedVariant) {
    // Mauvais variant → jouer animation "NO"
    Serial.printf("[GOTCHI-NFC] Mauvais variant! Attendu: %d, Recu: %d\n",
                  requestedVariant, badgeVariant);

    // Trouver et jouer l'animation "NO"
    if (EmotionManager::requestEmotion("NO", 1, EMOTION_PRIORITY_HIGH)) {
      Serial.println("[GOTCHI-NFC] Animation NO lancee");
    } else {
      Serial.println("[GOTCHI-NFC] ERREUR: Impossible de lancer animation NO");
    }

    return;
  }

  // Bon variant! → appliquer l'action et jouer l'animation eating
  Serial.printf("[GOTCHI-NFC] Bon variant! Application de l'action\n");

  // Obtenir l'action correspondante (bottle, snack, water, etc.)
  String action = getActionForVariant(badgeVariant);

  if (action.isEmpty()) {
    Serial.println("[GOTCHI-NFC] ERREUR: Action inconnue pour ce variant");
    return;
  }

  // Appliquer l'action via LifeManager
  if (LifeManager::applyAction(action)) {
    Serial.printf("[GOTCHI-NFC] Action '%s' appliquee avec succes\n", action.c_str());

    // Jouer l'animation "eating" correspondant au variant
    // Les animations eating doivent avoir été créées avec les bons variants (1-4)
    // dans la config côté serveur
    String emotionKey = "FOOD";  // Ou "EAT", selon votre convention

    if (EmotionManager::requestEmotion(emotionKey, 1, EMOTION_PRIORITY_HIGH)) {
      Serial.printf("[GOTCHI-NFC] Animation eating (variant %d) lancee\n", badgeVariant);
    } else {
      Serial.println("[GOTCHI-NFC] ERREUR: Impossible de lancer animation eating");
    }

  } else {
    // Action refusée (probablement cooldown actif)
    unsigned long remainingCooldown = LifeManager::getRemainingCooldown(action);
    Serial.printf("[GOTCHI-NFC] Action refusee (cooldown: %lu ms restants)\n", remainingCooldown);

    // Optionnel: jouer animation "wait" ou "NO"
    EmotionManager::requestEmotion("NO", 1, EMOTION_PRIORITY_HIGH);
  }

#endif
}

int GotchiNFCHandler::getVariantForBadge(uint8_t* uid, uint8_t uidLength) {
  // Variant 1: Biberon
  if (matchUID(uid, uidLength, BADGE_BOTTLE_UID, BADGE_BOTTLE_LEN)) {
    return 1;
  }

  // Variant 2: Gâteau
  if (matchUID(uid, uidLength, BADGE_CAKE_UID, BADGE_CAKE_LEN)) {
    return 2;
  }

  // Variant 3: Pomme
  if (matchUID(uid, uidLength, BADGE_APPLE_UID, BADGE_APPLE_LEN)) {
    return 3;
  }

  // Variant 4: Bonbon
  if (matchUID(uid, uidLength, BADGE_CANDY_UID, BADGE_CANDY_LEN)) {
    return 4;
  }

  // Badge inconnu
  return 0;
}

String GotchiNFCHandler::getActionForVariant(int variant) {
  switch (variant) {
    case 1:
      return "bottle";  // Biberon
    case 2:
      return "snack";   // Gâteau (snack)
    case 3:
      return "water";   // Pomme (TODO: créer action "apple" si différent de water)
    case 4:
      return "snack";   // Bonbon (snack)
    default:
      return "";
  }
}

bool GotchiNFCHandler::matchUID(uint8_t* uid1, uint8_t len1, const uint8_t* uid2, uint8_t len2) {
  if (len1 != len2) {
    return false;
  }

  for (uint8_t i = 0; i < len1; i++) {
    if (uid1[i] != uid2[i]) {
      return false;
    }
  }

  return true;
}

String GotchiNFCHandler::uidToString(uint8_t* uid, uint8_t uidLength) {
  String result = "";

  for (uint8_t i = 0; i < uidLength; i++) {
    if (uid[i] < 0x10) result += "0";
    result += String(uid[i], HEX);
    if (i < uidLength - 1) result += ":";
  }

  result.toUpperCase();
  return result;
}
