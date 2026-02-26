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
   * Arrêt complet : task, queue, BLEDevice::deinit.
   * Libère toute la RAM BLE. Utilisé par OTA et à la désactivation BLE (timeout).
   * Appeler init(getDeviceNameForReinit()) pour réactiver le BLE.
   */
  static void shutdownForOta();

  /**
   * Nom du device enregistré à l'init (pointeur stable, pour ré-init après shutdown).
   * @return Pointeur vers le nom, ou nullptr si jamais initialisé
   */
  static const char* getDeviceNameForReinit();
  
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
  /** Nom du device (pointeur passé à init(), non libéré) pour ré-init après shutdown */
  static const char* deviceNameForReinit;
};

#endif // BLE_MANAGER_H