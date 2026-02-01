#ifndef MODEL_BASIC_CONFIG_H
#define MODEL_BASIC_CONFIG_H

/**
 * Configuration du modèle Kidoo Basic
 * 
 * Ce fichier contient toutes les configurations spécifiques au modèle Basic :
 * - Pins GPIO pour les bandes LED
 * - Nombre de LEDs
 * - Configuration de la carte SD
 * - Composants disponibles
 */

// ============================================
// Configuration des bandes LED
// ============================================

// Pin de données pour la bande LED principale
// ESP32-S3-WROOM: Utiliser un GPIO accessible (pas 48 qui est la LED intégrée)
// GPIO 17 est généralement libre et fonctionne bien avec FastLED
#define LED_DATA_PIN 17

// Nombre de LEDs sur la bande principale
#define NUM_LEDS 42

// Type de LED (WS2812B, WS2811, etc.)
// Options: NEOPIXEL, WS2812B, WS2811, SK6812, etc.
#define LED_TYPE NEOPIXEL

// Ordre des couleurs (RGB, GRB, BGR, etc.)
// La plupart des WS2812B utilisent GRB
#define COLOR_ORDER GRB

// ============================================
// Configuration de la carte SD (SPI)
// ============================================

// Pins SPI pour la carte SD (ESP32-S3)
#define SD_MOSI_PIN 11      // GPIO 11 (SPI MOSI)
#define SD_MISO_PIN 13      // GPIO 13 (SPI MISO)
#define SD_SCK_PIN 12       // GPIO 12 (SPI SCK)
#define SD_CS_PIN 10        // GPIO 10 (Chip Select)

// ============================================
// Configuration NFC (PN532 via I2C)
// ============================================

// Pins I2C pour le module NFC PN532 (ESP32-S3)
#define NFC_SDA_PIN 8       // GPIO 8 (I2C SDA)
#define NFC_SCL_PIN 9       // GPIO 9 (I2C SCL)
#define NFC_IRQ_PIN 4       // GPIO 4 (Interrupt - optionnel)
#define NFC_RST_PIN 5       // GPIO 5 (Reset - optionnel)

// Adresse I2C du PN532 (généralement 0x24 en mode I2C)
#define NFC_I2C_ADDRESS 0x24

// ============================================
// Configuration RTC DS3231 (I2C)
// ============================================

// Le DS3231 utilise le même bus I2C que le NFC (ESP32-S3)
#define RTC_SDA_PIN 8       // GPIO 8 (I2C SDA)
#define RTC_SCL_PIN 9       // GPIO 9 (I2C SCL)

// Adresse I2C du DS3231 (fixe)
#define RTC_I2C_ADDRESS 0x68

// ============================================
// Configuration Potentiomètre WH148 (ADC)
// ============================================

// Pin analogique pour le potentiomètre (ESP32-S3)
#define POTENTIOMETER_PIN 1   // GPIO 1 (ADC1_CH0)

// ============================================
// Configuration Bouton BLE (Activation BLE)
// ============================================

// Pin GPIO pour le bouton d'activation BLE (INPUT_PULLUP)
// Appui long (3 secondes) pour activer le BLE
#define BLE_CONFIG_BUTTON_PIN 0   // GPIO 0 (ESP32-S3)

// ============================================
// Configuration Audio I2S (MAX98357)
// ============================================

// Pins I2S pour l'amplificateur audio (ESP32-S3)
// Note: GPIO 36-38 sont réservés pour PSRAM/Flash, ne pas utiliser !
#define I2S_BCLK_PIN 39       // GPIO 39 (Bit Clock)
#define I2S_LRC_PIN 40        // GPIO 40 (Left/Right Clock / Word Select)
#define I2S_DOUT_PIN 41       // GPIO 41 (Data Out)

// Volume par défaut en pourcentage (0-100%)
#define DEFAULT_AUDIO_VOLUME 25

// ============================================
// Composants disponibles sur ce modèle
// ============================================

#define HAS_SD_CARD true
#define HAS_LED true        // Désactivé pour test
#define HAS_WIFI true       // Désactivé pour test
#define HAS_BLE true
#define HAS_NFC true
#define HAS_PUBNUB true
#define HAS_RTC true
#define HAS_POTENTIOMETER false
#define HAS_AUDIO true

#endif // MODEL_BASIC_CONFIG_H
