#ifndef DEFAULT_CONFIG_SOUND_H
#define DEFAULT_CONFIG_SOUND_H

/**
 * Configuration par défaut du modèle Kidoo Sound
 *
 * Ces valeurs sont utilisées lorsque :
 * - Le fichier config.json n'existe pas sur la carte SD
 * - Le fichier config.json est invalide
 * - Première utilisation du Kidoo Sound
 */

// ============================================
// Configuration par défaut - Sound
// ============================================

// Nom du dispositif par défaut
#define DEFAULT_DEVICE_NAME "Kidoo-Sound"

// Configuration WiFi par défaut (vide = non configuré)
#define DEFAULT_WIFI_SSID ""
#define DEFAULT_WIFI_PASSWORD ""

// Luminosité LED par défaut (0-255)
#define DEFAULT_LED_BRIGHTNESS 128

// ============================================
// Configuration MQTT - Sound
// ============================================

// Version du firmware Kidoo (spécifique au modèle)
#define FIRMWARE_VERSION "1.0.0"

// MQTT broker URL is fetched from server via /api/devices/{MAC}/mqtt-token
// No fallback - server is the source of truth

#endif // DEFAULT_CONFIG_SOUND_H
