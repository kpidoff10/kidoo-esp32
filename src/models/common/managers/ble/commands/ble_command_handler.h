#ifndef BLE_COMMAND_HANDLER_H
#define BLE_COMMAND_HANDLER_H

#include <Arduino.h>

/**
 * Gestionnaire de commandes BLE
 * Route les commandes reçues vers les handlers appropriés
 */

class BLECommandHandler {
public:
  /**
   * Traiter une commande BLE reçue
   * @param data Données reçues (format JSON)
   * @return true si la commande a été traitée avec succès, false sinon
   */
  static bool handleCommand(const String& data);
  
  /**
   * Envoyer une réponse via BLE
   * @param success true si succès, false si erreur
   * @param message Message de réponse
   */
  static void sendResponse(bool success, const String& message);
  
  /**
   * Initialiser le handler avec la caractéristique TX
   * @param txCharacteristic Caractéristique TX pour envoyer les réponses
   */
  static void init(void* txCharacteristic);
};

#endif // BLE_COMMAND_HANDLER_H
