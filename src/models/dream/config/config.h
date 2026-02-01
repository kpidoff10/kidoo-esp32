#ifndef MODEL_DREAM_CONFIG_H
#define MODEL_DREAM_CONFIG_H

/**
 * Configuration du modèle Kidoo Dream (ESP32-S3)
 * 
 * ESP32-C3 : Single-core RISC-V 160MHz, pas de PSRAM
 * 
 * Ce fichier contient toutes les configurations spécifiques au modèle Mini :
 * - Pins GPIO pour les bandes LED
 * - Nombre de LEDs (réduit)
 * - Configuration de la carte SD
 * - Composants disponibles (limités par rapport au Basic)
 */

// ============================================
// Configuration des bandes LED
// ============================================

// Pin de données pour la bande LED principale
// ESP32-C3 : GPIO 8 est souvent utilisé pour les LEDs
#define LED_DATA_PIN 8

// Nombre de LEDs sur la bande principale (Mini a moins de LEDs)
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

// Pins SPI pour la carte SD (ESP32-C3)
// Note: ESP32-C3 a moins de GPIO disponibles
#define SD_MOSI_PIN 6       // GPIO 6 (SPI MOSI)
#define SD_MISO_PIN 5       // GPIO 5 (SPI MISO)
#define SD_SCK_PIN 4        // GPIO 4 (SPI SCK)
#define SD_CS_PIN 7         // GPIO 7 (Chip Select)

// ============================================
// Configuration RTC DS3231 (I2C) - Optionnel
// ============================================

// Le DS3231 utilise le bus I2C standard (ESP32-C3)
#define RTC_SDA_PIN 2       // GPIO 2 (I2C SDA)
#define RTC_SCL_PIN 3       // GPIO 3 (I2C SCL)

// Adresse I2C du DS3231 (fixe)
#define RTC_I2C_ADDRESS 0x68

// ============================================
// Configuration Bouton BLE (Activation BLE)
// ============================================

// Pin GPIO pour le bouton d'activation BLE (INPUT_PULLUP)
// Appui long (3 secondes) pour activer le BLE
// Note: GPIO 0 peut être un strapping pin, vérifier selon votre hardware
#define BLE_CONFIG_BUTTON_PIN 1   // GPIO 1 (ESP32-C3)

// ============================================
// Composants disponibles sur ce modèle
// ============================================
// ESP32-C3 : Single-core, pas de PSRAM, GPIO limités
// On désactive les composants gourmands en ressources

#define HAS_SD_CARD true
#define HAS_LED true
#define HAS_BLE true
#define HAS_RTC true
#define HAS_PUBNUB true

#endif // MODEL_DREAM_CONFIG_H
