#include "gotchi_nfc_handler.h"
#include "../../../../common/managers/nfc/nfc_manager.h"
#include "../emotions/emotion_manager.h"
#include "../emotions/trigger_manager.h"
#include "../life/life_manager.h"
#include "../../../model_config.h"

// Variables statiques
bool GotchiNFCHandler::initialized = false;
bool GotchiNFCHandler::tagPresent = false;
uint8_t GotchiNFCHandler::activeTagUID[10] = {0};
uint8_t GotchiNFCHandler::activeTagLength = 0;
// Action en cours pour la loop eating (bottle, cake, apple, candy) — utilisée pour arrêter l'effet au retrait du tag
static String s_currentLoopActionId;

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

// Variant depuis la clé écrite sur le tag (gotchi-nfc-write) - bloc 4
// On accepte préfixe ou égal (lecture bloc 4 peut être légèrement corrompue: "SNAC4..." au lieu de "SNACK")
static int getVariantForWrittenKey(const String& key) {
  String k = key;
  k.trim();
  if (k.length() < 2) return 0;
  if (k.startsWith("BOTTLE")) return 1;
  if (k.startsWith("SNACK") || k.startsWith("CAKE") || k.startsWith("SNAC")) return 2;  // snack/gâteau
  if (k.startsWith("APPLE")) return 3;
  if (k.startsWith("CANDY")) return 4;
  return 0;
}

// Nombre d'itérations de loop "illimité" pour le biberon (sortie par condition uniquement)
#define BOTTLE_LOOP_ITERATIONS 32767
// Faim à 100% = rassasié → on déclenche l'animation EXIT (fin) même si le NFC est encore posé
#define BOTTLE_HUNGER_SATIATED 100

// Condition de sortie de la loop biberon : on continue seulement si tag présent ET encore faim (< 100%)
static bool bottleLoopContinueCondition() {
  if (!GotchiNFCHandler::isTagPresent()) return false;  // Tag retiré → arrêter tout de suite
  GotchiStats stats = LifeManager::getStats();
  return (stats.hunger < BOTTLE_HUNGER_SATIATED);  // À 100% → EXIT automatique
}

// Condition de sortie pour cake/apple/candy : on continue tant que le tag est posé ET l'effet progressif est actif (jusqu'au cooldown / fin des ticks)
static bool foodLoopContinueCondition() {
  if (!GotchiNFCHandler::isTagPresent()) return false;
  return LifeManager::isProgressiveEffectActive(s_currentLoopActionId);
}

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
      // Arrêter l'effet progressif en cours (biberon, gâteau, etc.)
      if (!s_currentLoopActionId.isEmpty()) {
        LifeManager::stopProgressiveEffect(s_currentLoopActionId);
        s_currentLoopActionId = "";
      }
      // Forcer l'animation à jouer la phase EXIT puis s'arrêter
      EmotionManager::requestExitLoop();
    }
  }

#endif
}

bool GotchiNFCHandler::isTagPresent() {
  return tagPresent;
}

void GotchiNFCHandler::onTagDetected(uint8_t* uid, uint8_t uidLength) {
#if defined(HAS_NFC) && HAS_NFC

  Serial.print("[GOTCHI-NFC] Tag detecte: ");
  Serial.println(uidToString(uid, uidLength));

  // Sauvegarder l'UID du tag actif
  memcpy(activeTagUID, uid, uidLength);
  activeTagLength = uidLength;
  tagPresent = true;

  // Obtenir le variant du badge NFC (par UID prédéfini ou par clé écrite en bloc 4)
  int badgeVariant = getVariantForBadge(uid, uidLength);
  if (badgeVariant == 0) {
    uint8_t data[16];
    if (NFCManager::readBlock(4, data, uid, uidLength)) {
      // Priorité : code variant 1 octet (écrit par gotchi-nfc-write) = fiable
      if (data[0] >= 1 && data[0] <= 4) {
        badgeVariant = data[0];
        Serial.printf("[GOTCHI-NFC] Tag reconnu par code: %d\n", badgeVariant);
      } else {
        // Fallback : clé texte (anciens tags ou écriture manuelle)
        data[15] = '\0';
        String key = String((char*)data);
        key.trim();
        badgeVariant = getVariantForWrittenKey(key);
        if (badgeVariant != 0) {
          Serial.printf("[GOTCHI-NFC] Tag reconnu par cle ecrite: %s -> variant %d\n", key.c_str(), badgeVariant);
        } else {
          Serial.printf("[GOTCHI-NFC] Cle lue bloc 4 non reconnue: '%s' (attendu code 1-4 ou BOTTLE/SNACK/CAKE/APPLE/CANDY)\n", key.c_str());
        }
      }
    } else {
      Serial.println("[GOTCHI-NFC] Lecture bloc 4 echouee (garder le tag pose un peu plus longtemps?)");
    }
  }
  if (badgeVariant == 0) {
    Serial.println("[GOTCHI-NFC] Badge inconnu, aucune action");
    return;
  }

  Serial.printf("[GOTCHI-NFC] Badge variant: %d\n", badgeVariant);

  // Triggers faim : accepter n'importe quel badge aliment. Sinon variant demandé par le dernier trigger.
  int requestedVariant = TriggerManager::isAcceptAnyFoodTrigger() ? 0 : TriggerManager::getRequestedVariant();

  Serial.printf("[GOTCHI-NFC] Variant demande par Gotchi: %d\n", requestedVariant);

  // Si pas de variant demandé (trigger faim ou pas de trigger actif), accepter n'importe quel badge
  if (requestedVariant == 0) {
    Serial.println("[GOTCHI-NFC] Accepte le badge (faim = tout aliment, ou pas de demande)");
    requestedVariant = badgeVariant;  // Accepter
  }

  // Biberon (variant 1) : toujours accepter, même si la faim est à 100 % (pas de trigger faim actif)
  if (badgeVariant == 1) {
    requestedVariant = 1;
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

  // Obtenir l'action correspondante (bottle, cake, apple, candy)
  String action = getActionForVariant(badgeVariant);

  if (action.isEmpty()) {
    Serial.println("[GOTCHI-NFC] ERREUR: Action inconnue pour ce variant");
    return;
  }

  // Appliquer l'action via LifeManager
  if (LifeManager::applyAction(action)) {
    Serial.printf("[GOTCHI-NFC] Action '%s' appliquee avec succes\n", action.c_str());

    // Jouer l'animation "eating" (Mange) correspondant au variant (biberon=1, gâteau=2, pomme=3, bonbon=4)
    String emotionKey = "eating";

    // Biberon (variant 1) : boucle tant qu'il a encore faim OU que le tag est présent
    if (badgeVariant == 1) {
      s_currentLoopActionId = action;
      if (EmotionManager::requestEmotion(emotionKey, BOTTLE_LOOP_ITERATIONS, EMOTION_PRIORITY_HIGH, badgeVariant, "eating", bottleLoopContinueCondition)) {
        Serial.println("[GOTCHI-NFC] Animation biberon lancee (loop jusqu'a rassasiement ou tag retire)");
      } else {
        s_currentLoopActionId = "";
        Serial.println("[GOTCHI-NFC] ERREUR: Impossible de lancer animation eating");
      }
    } else {
      // Cake, apple, candy : boucle tant que le tag est posé et l'effet progressif actif (mange jusqu'au cooldown / fin des ticks)
      s_currentLoopActionId = action;
      if (EmotionManager::requestEmotion(emotionKey, BOTTLE_LOOP_ITERATIONS, EMOTION_PRIORITY_HIGH, badgeVariant, "eating", foodLoopContinueCondition)) {
        Serial.printf("[GOTCHI-NFC] Animation eating (variant %d) lancee en loop (tag pose jusqu'a fin effet)\n", badgeVariant);
      } else {
        s_currentLoopActionId = "";
        Serial.println("[GOTCHI-NFC] ERREUR: Impossible de lancer animation eating");
      }
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
    case 1: return "bottle";
    case 2: return "cake";    // Gâteau
    case 3: return "apple";   // Pomme
    case 4: return "candy";   // Bonbon
    default: return "";
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
