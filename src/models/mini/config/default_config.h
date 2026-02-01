#ifndef MODEL_MINI_DEFAULT_CONFIG_H
#define MODEL_MINI_DEFAULT_CONFIG_H

/**
 * Configuration par défaut du modèle Kidoo Mini
 * 
 * Ces valeurs sont utilisées lorsque :
 * - Le fichier config.json n'existe pas sur la carte SD
 * - Le fichier config.json est invalide
 * - Première utilisation du Kidoo Mini
 */

// ============================================
// Configuration par défaut - Mini
// ============================================

// Nom du dispositif par défaut
#define DEFAULT_DEVICE_NAME "Kidoo-Mini"

// Configuration WiFi par défaut (vide = non configuré)
#define DEFAULT_WIFI_SSID ""
#define DEFAULT_WIFI_PASSWORD ""

// Luminosité LED par défaut (0-255) - 50% = 128
#define DEFAULT_LED_BRIGHTNESS 128

// ============================================
// Configuration PubNub - Mini
// ============================================

// Clés PubNub (créer un compte gratuit sur https://www.pubnub.com/)
// Subscribe Key (obligatoire pour recevoir des messages)
#define DEFAULT_PUBNUB_SUBSCRIBE_KEY ""

// Publish Key (obligatoire pour envoyer des messages)
#define DEFAULT_PUBNUB_PUBLISH_KEY ""

#endif // MODEL_MINI_DEFAULT_CONFIG_H
