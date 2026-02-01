#ifndef INIT_MANAGER_H
#define INIT_MANAGER_H

#include <Arduino.h>

// Forward declaration
struct SDConfig;

/**
 * Gestionnaire d'initialisation du système
 * 
 * Ce module centralise l'initialisation de tous les composants
 * et managers du système dans le bon ordre.
 */

// État d'initialisation d'un composant
enum InitStatus {
  INIT_NOT_STARTED,    // Pas encore initialisé
  INIT_IN_PROGRESS,    // Initialisation en cours
  INIT_SUCCESS,        // Initialisation réussie
  INIT_FAILED          // Échec de l'initialisation
};

// État global du système
struct SystemStatus {
  InitStatus serial;        // Communication série
  InitStatus led;           // Gestionnaire LED
  InitStatus sd;            // Gestionnaire SD
  InitStatus nfc;           // Gestionnaire NFC
  InitStatus ble;           // Gestionnaire BLE
  InitStatus wifi;          // Gestionnaire WiFi
  InitStatus pubnub;        // Gestionnaire PubNub
  InitStatus rtc;           // Gestionnaire RTC DS3231
  InitStatus potentiometer; // Gestionnaire Potentiomètre
  InitStatus audio;         // Gestionnaire Audio I2S
  // Ajouter d'autres composants ici
  
  bool isFullyInitialized() const {
    return serial == INIT_SUCCESS && led == INIT_SUCCESS;
  }
};

class InitManager {
public:
  // Initialiser tous les composants du système
  static bool init();
  
  // Obtenir l'état global du système
  static SystemStatus getStatus();
  
  // Vérifier si le système est complètement initialisé
  static bool isSystemReady();
  
  // Obtenir le statut d'un composant spécifique
  static InitStatus getComponentStatus(const char* componentName);
  
  // Afficher le statut de tous les composants (pour debug)
  static void printStatus();
  
  // Configuration globale
  // Obtenir la configuration système (accessible de n'importe où)
  static const SDConfig& getConfig();
  
  // Vérifier si la configuration est valide
  static bool isConfigValid();
  
  // Stocker la configuration globalement (utilisé en interne)
  static void setGlobalConfig(SDConfig* config);
  
  // Mettre à jour et sauvegarder la configuration
  static bool updateConfig(const SDConfig& config);

private:
  // Initialiser chaque composant individuellement
  static bool initSerial();
  static bool initLED();
  static bool initSD();
  static bool initNFC();
  static bool initBLE();
  static bool initWiFi();
  static bool initPubNub();
  static bool initRTC();
  static bool initPotentiometer();
  static bool initAudio();
  // Ajouter d'autres méthodes d'initialisation ici
  
  // Variables statiques
  static SystemStatus systemStatus;
  static bool initialized;
  static SDConfig* globalConfig;  // Configuration globale du système
  
  // Configuration Serial
  static const unsigned long SERIAL_BAUD_RATE = 115200;
  static const unsigned long SERIAL_TIMEOUT_MS = 3000;
};

#endif // INIT_MANAGER_H
