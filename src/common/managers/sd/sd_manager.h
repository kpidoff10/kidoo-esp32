#ifndef SD_MANAGER_H
#define SD_MANAGER_H

#include <Arduino.h>

/**
 * Gestionnaire de carte SD
 * 
 * Ce module gère l'initialisation et l'accès à la carte SD
 */

// Structure pour stocker la configuration
struct SDConfig {
  bool valid;              // Indique si la configuration est valide
  char wifi_ssid[64];      // SSID WiFi
  char wifi_password[64];  // Mot de passe WiFi
  char device_name[32];    // Nom du dispositif
  uint8_t led_brightness;   // Luminosité LED (0-255)
  uint32_t sleep_timeout_ms; // Timeout pour le sleep mode (0 = désactivé)
  // Configuration bedtime (modèle Dream uniquement)
  uint8_t bedtime_colorR;    // Couleur R pour bedtime (0-255)
  uint8_t bedtime_colorG;   // Couleur G pour bedtime (0-255)
  uint8_t bedtime_colorB;   // Couleur B pour bedtime (0-255)
  uint8_t bedtime_brightness; // Luminosité pour bedtime (0-100)
  bool bedtime_allNight;    // Veilleuse toute la nuit
  char bedtime_effect[32];  // Effet LED pour bedtime ("none", "pulse", "rainbow-soft", "breathe", "nightlight", etc.)
  char bedtime_weekdaySchedule[512]; // Schedule par jour (JSON sérialisé: {"monday":{"hour":20,"minute":0,"activated":true},...})
  // Configuration wakeup (modèle Dream uniquement)
  uint8_t wakeup_colorR;    // Couleur R pour wakeup (0-255)
  uint8_t wakeup_colorG;   // Couleur G pour wakeup (0-255)
  uint8_t wakeup_colorB;   // Couleur B pour wakeup (0-255)
  uint8_t wakeup_brightness; // Luminosité pour wakeup (0-100)
  char wakeup_weekdaySchedule[512]; // Schedule par jour (JSON sérialisé: {"monday":{"hour":7,"minute":30,"activated":true},...})
  // Note: MQTT est configuré dans default_config.h (pas sur SD)
};

class SDManager {
public:
  // Initialiser le gestionnaire SD
  static bool init();
  
  // Tester si la carte SD est disponible
  static bool isAvailable();
  
  // Obtenir le type de carte (CARD_NONE, CARD_MMC, CARD_SD, CARD_SDHC)
  static uint8_t getCardType();
  
  // Obtenir l'espace total (en octets)
  static uint64_t getTotalSpace();
  
  // Obtenir l'espace libre (en octets)
  static uint64_t getFreeSpace();
  
  // Obtenir l'espace utilisé (en octets)
  static uint64_t getUsedSpace();
  
  // Lire la configuration depuis config.json
  static SDConfig getConfig();
  
  // Vérifier si le fichier config.json existe
  static bool configFileExists();
  
  // Initialiser une configuration avec les valeurs par défaut
  static void initDefaultConfig(SDConfig* config);
  
  // Sauvegarder la configuration dans config.json
  static bool saveConfig(const SDConfig& config);

private:
  // Initialiser la carte SD avec les pins configurés
  static bool initSDCard();
  
  // Variables statiques
  static bool initialized;
  static bool cardAvailable;
  
  // Chemin du fichier de configuration
  static const char* CONFIG_FILE_PATH;
};

#endif // SD_MANAGER_H
