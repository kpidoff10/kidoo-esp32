#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include "../../config/core_config.h"

#ifdef HAS_WIFI
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

/**
 * Gestionnaire WiFi commun (Core 0)
 * 
 * Ce module gère l'initialisation et les opérations WiFi
 * pour tous les modèles supportant le WiFi.
 * 
 * Architecture :
 * - Le WiFi stack ESP-IDF tourne automatiquement sur Core 0
 * - Le thread de retry tourne aussi sur Core 0 (CORE_WIFI_RETRY)
 * - Priorité très basse (PRIORITY_WIFI_RETRY) pour ne pas bloquer les autres tâches
 */

// États de connexion WiFi
enum WiFiConnectionStatus {
  WIFI_STATUS_DISCONNECTED,    // Non connecté
  WIFI_STATUS_CONNECTING,      // Connexion en cours
  WIFI_STATUS_CONNECTED,       // Connecté
  WIFI_STATUS_CONNECTION_FAILED // Échec de connexion
};

class WiFiManager {
public:
  /**
   * Initialiser le gestionnaire WiFi
   * @return true si l'initialisation est réussie, false sinon
   */
  static bool init();
  
  /**
   * Vérifier si le WiFi est disponible/opérationnel
   * @return true si le WiFi est opérationnel, false sinon
   */
  static bool isAvailable();
  
  /**
   * Vérifier si le WiFi est initialisé
   * @return true si le WiFi est initialisé, false sinon
   */
  static bool isInitialized();
  
  /**
   * Se connecter au WiFi avec les identifiants de la config
   * @return true si la connexion est réussie, false sinon
   */
  static bool connect();
  
  /**
   * Se connecter au WiFi avec des identifiants spécifiques
   * @param ssid SSID du réseau
   * @param password Mot de passe du réseau
   * @param timeoutMs Timeout en millisecondes (défaut: 10000)
   * @return true si la connexion est réussie, false sinon
   */
  static bool connect(const char* ssid, const char* password, uint32_t timeoutMs = 10000);
  
  /**
   * Se déconnecter du WiFi
   */
  static void disconnect();
  
  /**
   * Vérifier si le WiFi est connecté
   * @return true si connecté, false sinon
   */
  static bool isConnected();
  
  /**
   * Obtenir l'état de connexion
   * @return État de connexion WiFi
   */
  static WiFiConnectionStatus getConnectionStatus();
  
  /**
   * Obtenir l'adresse IP locale
   * @return Adresse IP sous forme de String
   */
  static String getLocalIP();
  
  /**
   * Obtenir le SSID du réseau connecté
   * @return SSID du réseau
   */
  static String getSSID();
  
  /**
   * Obtenir la force du signal (RSSI)
   * @return RSSI en dBm
   */
  static int getRSSI();
  
  /**
   * Afficher les informations WiFi sur Serial
   */
  static void printInfo();
  
  /**
   * Démarrer le thread de retry automatique WiFi
   * Le thread tentera de se connecter avec un backoff exponentiel
   * (5s, 10s, 15s, 20s, 30s, 40s, 50s, 60s max)
   * Arrête après 1 minute sans connexion
   */
  static void startRetryThread();
  
  /**
   * Arrêter le thread de retry automatique WiFi
   */
  static void stopRetryThread();
  
  /**
   * Vérifier si le thread de retry est actif
   * @return true si le thread est actif
   */
  static bool isRetryThreadActive();

private:
  // Fonction du thread FreeRTOS pour le retry
  static void retryThreadFunction(void* parameter);
  
  // Variables statiques
  static bool initialized;
  static bool available;
  static WiFiConnectionStatus connectionStatus;
  static char currentSSID[64];
  
  // Thread de retry
  static TaskHandle_t retryTaskHandle;
  static bool retryThreadRunning;
  static unsigned long retryStartTime;
  
  // Timeout de connexion par défaut (15 secondes)
  static const uint32_t DEFAULT_CONNECT_TIMEOUT_MS = 15000;
  
  // Configuration du retry (centralisée dans core_config.h)
  static const uint32_t RETRY_MAX_DURATION_MS = 60000;  // 1 minute max
  static const uint32_t RETRY_INITIAL_DELAY_MS = 5000;  // 5 secondes initial
  static const uint32_t RETRY_MAX_DELAY_MS = 60000;     // 60 secondes max entre tentatives
  static const int RETRY_STACK_SIZE = STACK_SIZE_WIFI_RETRY;
  static const int RETRY_TASK_PRIORITY = PRIORITY_WIFI_RETRY;
  static const int RETRY_TASK_CORE = CORE_WIFI_RETRY;   // Core 0 avec WiFi stack
};

#endif // WIFI_MANAGER_H
