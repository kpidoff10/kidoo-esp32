#ifndef MODEL_GOTCHI_CONFIG_H
#define MODEL_GOTCHI_CONFIG_H

/**
 * Configuration du modèle Kidoo Gotchi (ESP32-S3-N16R8)
 *
 * ESP32-S3 : Dual-core, 16MB Flash, 8MB PSRAM OPI
 *
 * Ce fichier contient les configurations spécifiques au modèle Gotchi :
 * - Pins GPIO pour les bandes LED
 * - Configuration de la carte SD
 * - Composants disponibles
 */

// ============================================
// Configuration LED - LED intégrée ESP32-S3
// ============================================
// DevKitC-1 : LED RGB WS2812 intégrée. La plupart des cartes : GPIO 48.
// Certaines révisions (v1.1) utilisent GPIO 38 — si la LED ne s'allume pas, essayer 38.

#define LED_DATA_PIN 48
#define NUM_LEDS 1

#define LED_TYPE NEOPIXEL
#define COLOR_ORDER GRB

// ============================================
// Configuration de la carte SD (SPI)
// ============================================

#define SD_MOSI_PIN 11
#define SD_MISO_PIN 13
#define SD_SCK_PIN 12
#define SD_CS_PIN 10

// ============================================
// Configuration RTC DS3231 (I2C)
// ============================================

#define RTC_SDA_PIN 8
#define RTC_SCL_PIN 9
#define RTC_I2C_ADDRESS 0x68

// ============================================
// Configuration Bouton BLE (Activation BLE)
// ============================================

#define BLE_CONFIG_BUTTON_PIN 0

// ============================================
// Configuration LCD SPI (ST7789)
// ============================================
// Brochage module : GND, VCC 3.3V, SCL, SDA, RES, DC, CS, BLK
// SDA = données SPI (MOSI), SCL = horloge SPI (SCK)
// BLK = rétroéclairage (optionnel, LOW = éteint)
//
// Résolution / rotation : si tu vois des barres ou un écran "cassé", essaie :
// - TFT 240x320 (très courant) au lieu de 240x280
// - TFT_ROTATION 0 ou 1 au lieu de 2

#define TFT_CS_PIN   14   // Pin 7 - Chip Select
#define TFT_DC_PIN   15   // Pin 6 - Data/Command
#define TFT_RST_PIN  16   // Pin 5 - Reset (RES)
#define TFT_MOSI_PIN 11   // Pin 4 - SDA (SPI data)
#define TFT_SCK_PIN  12   // Pin 3 - SCL (SPI clock)
#define TFT_BLK_PIN  7    // Pin 8 - Rétroéclairage (optionnel)

// Panel physique ST7789 : 240x280. Rotation 1 = landscape (280x240 logique)
// Les vidéos sont pivotées côté serveur (transpose=1) pour s'afficher en landscape
#define TFT_WIDTH    240
#define TFT_HEIGHT   280
#define TFT_ROTATION 1  // Landscape 90° (280x240)
// Offset écran : en landscape, l'offset Y=20 du portrait devient X=20
#define TFT_OFFSET_X 0
#define TFT_OFFSET_Y 20

// ============================================
// Composants disponibles sur ce modèle
// ============================================
// HAS_WIFI et HAS_SD sont définis dans platformio.ini (build_flags) pour gotchi — ne pas les redéfinir ici.

#define HAS_SD_CARD true
#define HAS_LED true
#define HAS_LCD true
#define HAS_BLE true
#define HAS_PUBNUB true
#define HAS_RTC true

#endif // MODEL_GOTCHI_CONFIG_H
