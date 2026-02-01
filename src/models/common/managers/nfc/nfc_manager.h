#ifndef NFC_MANAGER_H
#define NFC_MANAGER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

/**
 * Gestionnaire NFC avec thread dédié
 * 
 * Ce module gère l'initialisation et les opérations NFC dans un thread séparé
 * pour ne pas bloquer l'audio ou les autres tâches.
 * 
 * Le thread détecte automatiquement les tags en arrière-plan et appelle
 * un callback quand un tag est détecté.
 */

// Callback appelé quand un tag est détecté
// uid: buffer contenant l'UID, uidLength: longueur de l'UID
typedef void (*NFCTagCallback)(uint8_t* uid, uint8_t uidLength);

class NFCManager {
public:
  /**
   * Initialiser le gestionnaire NFC et démarrer le thread de détection
   * @return true si l'initialisation est réussie, false sinon
   */
  static bool init();
  
  /**
   * Vérifier si le NFC est disponible/opérationnel
   * @return true si le NFC est opérationnel, false sinon
   */
  static bool isAvailable();
  
  /**
   * Vérifier si le NFC est initialisé
   * @return true si le NFC est initialisé, false sinon
   */
  static bool isInitialized();
  
  /**
   * Obtenir la version du firmware du module NFC (si disponible)
   * @return Version du firmware (0 si indisponible)
   */
  static uint32_t getFirmwareVersion();
  
  // ============================================
  // Détection automatique (thread)
  // ============================================
  
  /**
   * Définir le callback appelé quand un tag est détecté
   * @param callback Fonction à appeler (nullptr pour désactiver)
   */
  static void setTagCallback(NFCTagCallback callback);
  
  /**
   * Activer/désactiver la détection automatique
   * @param enabled true pour activer, false pour désactiver
   */
  static void setAutoDetect(bool enabled);
  
  /**
   * Vérifier si la détection automatique est active
   */
  static bool isAutoDetectEnabled();
  
  /**
   * Vérifier si un tag a été détecté récemment
   * @return true si un tag est présent
   */
  static bool isTagPresent();
  
  /**
   * Obtenir l'UID du dernier tag détecté
   * @param uid Buffer pour stocker l'UID (min 10 bytes)
   * @param uidLength Pointeur pour stocker la longueur
   * @return true si un tag a été détecté, false sinon
   */
  static bool getLastTagUID(uint8_t* uid, uint8_t* uidLength);
  
  // ============================================
  // Opérations manuelles (bloquantes mais courtes)
  // ============================================
  
  /**
   * Lire l'UID d'un tag NFC (appel bloquant)
   * Note: Préférer la détection automatique pour ne pas bloquer
   * @param uid Buffer pour stocker l'UID (max 10 bytes)
   * @param uidLength Pointeur pour stocker la longueur de l'UID
   * @param timeoutMs Timeout en millisecondes (défaut: 5000)
   * @return true si un tag a été détecté et lu, false sinon
   */
  static bool readTagUID(uint8_t* uid, uint8_t* uidLength, uint32_t timeoutMs = 5000);
  
  /**
   * Lire un bloc de données d'un tag NFC MIFARE Classic
   * @param blockNumber Numéro du bloc à lire (0-63)
   * @param data Buffer pour stocker les données (16 bytes)
   * @param uid UID du tag (doit être lu au préalable)
   * @param uidLength Longueur de l'UID
   * @return true si la lecture a réussi, false sinon
   */
  static bool readBlock(uint8_t blockNumber, uint8_t* data, uint8_t* uid, uint8_t uidLength);
  
  /**
   * Écrire un bloc de données sur un tag NFC MIFARE Classic
   * @param blockNumber Numéro du bloc à écrire (0-63)
   * @param data Données à écrire (16 bytes)
   * @param uid UID du tag (doit être lu au préalable)
   * @param uidLength Longueur de l'UID
   * @return true si l'écriture a réussi, false sinon
   */
  static bool writeBlock(uint8_t blockNumber, uint8_t* data, uint8_t* uid, uint8_t uidLength);

private:
  /**
   * Tester le hardware NFC
   * @return true si le hardware répond, false sinon
   */
  static bool testHardware();
  
  /**
   * Thread de détection NFC
   */
  static void nfcTask(void* parameter);
  
  // Variables statiques
  static bool initialized;
  static bool available;
  static uint32_t firmwareVersion;
  
  // Thread
  static TaskHandle_t taskHandle;
  static SemaphoreHandle_t nfcMutex;
  static volatile bool threadRunning;
  static volatile bool autoDetectEnabled;
  
  // Dernier tag détecté
  static uint8_t lastUID[10];
  static uint8_t lastUIDLength;
  static volatile bool tagPresent;
  static unsigned long lastDetectionTime;
  
  // Callback
  static NFCTagCallback tagCallback;
};

#endif // NFC_MANAGER_H
