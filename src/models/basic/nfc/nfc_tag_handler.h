#ifndef NFC_TAG_HANDLER_BASIC_H
#define NFC_TAG_HANDLER_BASIC_H

#include <Arduino.h>

/**
 * Gestionnaire de tags NFC pour le modèle Basic
 * 
 * Gère les actions associées aux tags NFC :
 * - Lancer/arrêter la musique
 * - Changer les LEDs
 * - etc.
 */

class NFCTagHandler {
public:
  /**
   * Initialiser le gestionnaire de tags
   * Configure le callback NFC pour la détection automatique
   */
  static void init();
  
  /**
   * Mettre à jour le gestionnaire (appelé dans loop())
   * Vérifie si un tag a été retiré et arrête la musique si nécessaire
   */
  static void update();
  
  /**
   * Callback appelé quand un tag est détecté
   */
  static void onTagDetected(uint8_t* uid, uint8_t uidLength);
  
  /**
   * Vérifier si un tag spécifique correspond à un UID donné
   */
  static bool matchUID(uint8_t* uid, uint8_t uidLength, const uint8_t* targetUID, uint8_t targetLength);
  
  /**
   * Convertir un UID en string hex (pour debug)
   */
  static String uidToString(uint8_t* uid, uint8_t uidLength);

private:
  static bool initialized;
  static bool musicPlaying;        // La musique joue-t-elle à cause d'un tag ?
  static uint8_t activeTagUID[10]; // UID du tag actif
  static uint8_t activeTagLength;  // Longueur de l'UID actif
};

#endif // NFC_TAG_HANDLER_BASIC_H
