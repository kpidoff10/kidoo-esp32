#ifndef MODEL_DREAM_DEFAULT_CONFIG_H
#define MODEL_DREAM_DEFAULT_CONFIG_H

/**
 * Configuration par défaut du modèle Kidoo Dream
 * 
 * Ces valeurs sont utilisées lorsque :
 * - Le fichier config.json n'existe pas sur la carte SD
 * - Le fichier config.json est invalide
 * - Première utilisation du Kidoo Dream
 */

// ============================================
// Configuration par défaut - Dream
// ============================================

// Nom du dispositif par défaut
#define DEFAULT_DEVICE_NAME "Kidoo-Dream"

// Configuration WiFi par défaut (vide = non configuré)
#define DEFAULT_WIFI_SSID ""
#define DEFAULT_WIFI_PASSWORD ""

// Luminosité LED par défaut (0-255) - 50% = 128
#define DEFAULT_LED_BRIGHTNESS 128

// ============================================
// Configuration MQTT - Dream
// ============================================

// Version du firmware Kidoo (spécifique au modèle)
#define FIRMWARE_VERSION "1.0.43"

// MQTT broker URL is fetched from server via /api/devices/{MAC}/mqtt-token
// No fallback - server is the source of truth

#endif // MODEL_DREAM_DEFAULT_CONFIG_H
