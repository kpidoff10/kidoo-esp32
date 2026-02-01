#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <Arduino.h>

/**
 * Gestionnaire BLE commun
 * 
 * Ce module gère l'initialisation et les opérations BLE
 * pour tous les modèles supportant le BLE
 */

class BLEManager {
public:
  /**
   * Initialiser le gestionnaire BLE
   * @param deviceName Nom du périphérique BLE
   * @return true si l'initialisation est réussie, false sinon
   */
  static bool init(const char* deviceName);
  
  /**
   * Vérifier si le BLE est disponible/opérationnel
   * @return true si le BLE est opérationnel, false sinon
   */
  static bool isAvailable();
  
  /**
   * Vérifier si le BLE est initialisé
   * @return true si le BLE est initialisé, false sinon
   */
  static bool isInitialized();
  
  /**
   * Démarre l'advertising BLE
   */
  static void startAdvertising();
  
  /**
   * Arrête l'advertising BLE
   */
  static void stopAdvertising();
  
  /**
   * Vérifier si un client BLE est connecté
   * @return true si un client est connecté, false sinon
   */
  static bool isConnected();
  
  /**
   * Obtenir le nom du dispositif BLE
   * @return Pointeur vers le nom du dispositif, ou nullptr si non initialisé
   */
  static const char* getDeviceName();

private:
  // Variables statiques
  static bool initialized;
  static bool available;
  static char* deviceName;
};

#endif // BLE_MANAGER_H