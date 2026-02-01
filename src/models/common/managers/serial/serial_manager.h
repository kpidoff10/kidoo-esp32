#ifndef SERIAL_MANAGER_H
#define SERIAL_MANAGER_H

#include <Arduino.h>

/**
 * Gestionnaire Serial avec fonctions utilitaires
 * 
 * Ce module fournit des fonctions utilitaires pour le Serial
 * et des fonctions système de base (reboot, etc.)
 */

class SerialManager {
public:
  /**
   * Initialiser le Serial Manager
   * Note: Serial.begin() doit être appelé avant (dans InitManager)
   */
  static void init();
  
  /**
   * Vérifier si Serial est disponible
   * @return true si Serial est disponible, false sinon
   */
  static bool isAvailable();
  
  /**
   * Redémarrer l'ESP32
   * @param delayMs Délai avant le reboot (en millisecondes)
   */
  static void reboot(uint32_t delayMs = 0);
  
  /**
   * Redémarrer l'ESP32 en mode deep sleep
   * @param delayMs Délai avant le deep sleep (en millisecondes)
   */
  static void deepSleep(uint32_t delayMs = 0);
  
  /**
   * Afficher les informations système
   */
  static void printSystemInfo();
  
  /**
   * Afficher l'utilisation de la mémoire
   */
  static void printMemoryInfo();
  
  /**
   * Afficher l'utilisation de la mémoire libre
   * @return Mémoire libre en octets
   */
  static uint32_t getFreeHeap();
  
  /**
   * Afficher l'utilisation de la mémoire totale
   * @return Mémoire totale en octets
   */
  static uint32_t getTotalHeap();
  
  /**
   * Afficher l'utilisation de la mémoire minimale jamais atteinte
   * @return Mémoire minimale en octets
   */
  static uint32_t getMinFreeHeap();
  
  /**
   * Formater et afficher un message avec timestamp
   * @param format Format du message (comme printf)
   * @param ... Arguments variables
   */
  static void log(const char* format, ...);
  
  /**
   * Afficher un message d'erreur avec timestamp
   * @param format Format du message (comme printf)
   * @param ... Arguments variables
   */
  static void logError(const char* format, ...);
  
  /**
   * Afficher un message de debug avec timestamp
   * @param format Format du message (comme printf)
   * @param ... Arguments variables
   */
  static void logDebug(const char* format, ...);

private:
  static bool initialized;
  static void printTimestamp();
};

#endif // SERIAL_MANAGER_H
