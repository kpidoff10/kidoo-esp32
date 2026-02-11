#ifndef GOTCHI_NFC_HANDLER_H
#define GOTCHI_NFC_HANDLER_H

#include <Arduino.h>

/**
 * Gestionnaire NFC spécifique pour le modèle Gotchi
 *
 * Gère les badges NFC de nourriture avec système de variants:
 * - Variant 1 (bottle): biberon
 * - Variant 2 (cake): gâteau
 * - Variant 3 (apple): pomme
 * - Variant 4 (candy): bonbon
 *
 * Logique:
 * 1. Gotchi demande un aliment (trigger hunger_low avec variant aléatoire 1-4)
 * 2. Si badge NFC != variant demandé → animation "NO"
 * 3. Si badge NFC == variant demandé → LifeManager.applyAction() + animation eating
 */

class GotchiNFCHandler {
public:
  /**
   * Initialiser le gestionnaire NFC Gotchi
   * Configure le callback NFC et les mappings badge→variant
   */
  static void init();

  /**
   * Mettre à jour le gestionnaire (à appeler dans loop())
   * Surveille si le tag est toujours présent
   */
  static void update();

  /** Indique si un tag NFC est actuellement présent (ex. biberon posé). */
  static bool isTagPresent();

private:
  static bool initialized;
  static bool tagPresent;
  static uint8_t activeTagUID[10];
  static uint8_t activeTagLength;

  /**
   * Callback appelé quand un tag NFC est détecté
   * @param uid Tableau contenant l'UID du tag
   * @param uidLength Longueur de l'UID
   */
  static void onTagDetected(uint8_t* uid, uint8_t uidLength);

  /**
   * Comparer deux UIDs
   * @param uid1 Premier UID
   * @param len1 Longueur du premier UID
   * @param uid2 Deuxième UID
   * @param len2 Longueur du deuxième UID
   * @return true si identiques, false sinon
   */
  static bool matchUID(uint8_t* uid1, uint8_t len1, const uint8_t* uid2, uint8_t len2);

  /**
   * Convertir un UID en String lisible (format HEX)
   * @param uid Tableau contenant l'UID
   * @param uidLength Longueur de l'UID
   * @return String au format "AA:BB:CC:DD"
   */
  static String uidToString(uint8_t* uid, uint8_t uidLength);

  /**
   * Obtenir le variant associé à un badge NFC
   * @param uid UID du badge
   * @param uidLength Longueur de l'UID
   * @return Variant (1-4), ou 0 si badge inconnu
   */
  static int getVariantForBadge(uint8_t* uid, uint8_t uidLength);

  /**
   * Obtenir l'action ID correspondant à un variant
   * @param variant Variant (1-4)
   * @return Action ID pour LifeManager ("bottle", "snack", etc.)
   */
  static String getActionForVariant(int variant);
};

#endif // GOTCHI_NFC_HANDLER_H
