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
// Configuration PubNub - Dream
// ============================================

// Clés PubNub (créer un compte gratuit sur https://www.pubnub.com/)
// Subscribe Key (obligatoire pour recevoir des messages)
#define DEFAULT_PUBNUB_SUBSCRIBE_KEY "sub-c-5f6c027d-31ec-4d2d-96f4-f7a63fa5e747"

// Publish Key (obligatoire pour envoyer des messages)
#define DEFAULT_PUBNUB_PUBLISH_KEY "pub-c-54932998-4a9f-44da-acd1-dface15b1cb7"

#endif // MODEL_DREAM_DEFAULT_CONFIG_H
