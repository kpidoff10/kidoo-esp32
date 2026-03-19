#ifndef CONFIG_SOUND_H
#define CONFIG_SOUND_H

#include <Arduino.h>

/**
 * Configuration matérielle du modèle Kidoo Sound (ESP32-S3)
 * Boîte à musique avec 64 LEDs WS2812, WiFi, BLE, RTC, SD, PSRAM
 */

// ============================================
// Configuration LED WS2812 (64 LEDs)
// ============================================
// GPIO 6 pour bande LED WS2812 64 LEDs (compatible RMT)

#define LED_DATA_PIN 6
#define NUM_LEDS 30
#define LED_TYPE NEOPIXEL
#define COLOR_ORDER GRB

// ============================================
// Configuration BLE - Bouton de configuration
// ============================================
// GPIO 14 pour bouton de configuration BLE (GPIO 0 = BOOT sur S3)

#define BLE_CONFIG_BUTTON_PIN 14

// ============================================
// Configuration RTC DS3231 (I2C)
// ============================================
// I2C standard: GPIO 8 (SDA), GPIO 9 (SCL)

#define RTC_SDA_PIN 8
#define RTC_SCL_PIN 9
#define RTC_I2C_ADDRESS 0x68

// ============================================
// Configuration de la carte SD (SPI)
// ============================================
// HSPI standard (ESP32-S3): GPIO 11 (MOSI), GPIO 13 (MISO), GPIO 12 (SCK), GPIO 10 (CS)

#define SD_MOSI_PIN 11
#define SD_MISO_PIN 13
#define SD_SCK_PIN 12
#define SD_CS_PIN 10

// ============================================
// Configuration I2S Audio - MAX98357A (Amplificateur audio)
// ============================================
// I2S Pins pour MAX98357A sur ESP32-S3
#define I2S_BCLK_PIN  45   // Bit Clock
#define I2S_LRC_PIN   46   // Left/Right Clock
#define I2S_DOUT_PIN  3    // Data Out (vers MAX98357A DIN)

// ============================================
// Composants disponibles sur ce modèle
// ============================================

#define HAS_AUDIO true

#ifndef HAS_SD_CARD
#define HAS_SD_CARD true
#endif

#ifndef HAS_RTC
#define HAS_RTC true
#endif

#ifndef HAS_BLE
#define HAS_BLE true
#endif

#ifndef HAS_WIFI
#define HAS_WIFI true
#endif

#ifndef HAS_LED
#define HAS_LED true
#endif

#ifndef HAS_MQTT
#define HAS_MQTT true
#endif

#endif // CONFIG_SOUND_H
