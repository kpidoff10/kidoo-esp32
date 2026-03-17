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
// Configuration PubNub - Sound
// ============================================

// Version du firmware Kidoo (spécifique au modèle)
#define FIRMWARE_VERSION "1.0.0"

// Clés PubNub (créer un compte gratuit sur https://www.pubnub.com/)
// Subscribe Key (obligatoire pour recevoir des messages)
#define DEFAULT_PUBNUB_SUBSCRIBE_KEY "sub-c-d0d7ee43-448a-4a84-a732-c935c7026e10"

// Publish Key (obligatoire pour envoyer des messages)
#define DEFAULT_PUBNUB_PUBLISH_KEY "pub-c-ca8e9630-69f5-48da-a032-eb03eb3941aa"

#endif // DEFAULT_CONFIG_SOUND_H
