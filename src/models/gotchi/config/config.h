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
// Configuration NFC (PN532 via I2C)
// ============================================

// I2C pins for NFC PN532 module (ESP32-S3)
// Module 4 pins: SDA, SCL, VCC (3.3V), GND
// NFC shares the I2C bus with RTC (same pins)

#define NFC_SDA_PIN 8       // GPIO 8 (I2C SDA - shared with RTC)
#define NFC_SCL_PIN 9       // GPIO 9 (I2C SCL - shared with RTC)

// I2C address of PN532 (usually 0x24 in I2C mode)
#define NFC_I2C_ADDRESS 0x24

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
#define HAS_NFC true
#define HAS_PUBNUB true
#define HAS_RTC true

// ============================================
// Configuration du système de vie (Tamagotchi)
// ============================================

// --- Limites des stats ---
#define STATS_MIN 0
#define STATS_MAX 100

// --- Initial stats ---
#define STATS_HUNGER_INITIAL 100      // Hunger initial value (100 = not hungry)
#define STATS_HAPPINESS_INITIAL 100   // Happiness initial value
#define STATS_HEALTH_INITIAL 100      // Health initial value
#define STATS_FATIGUE_INITIAL 0       // Fatigue initial value (0 = not tired)
#define STATS_HYGIENE_INITIAL 100     // Hygiene initial value

// --- Stats decline rates (points lost every 30 minutes) ---
#define STATS_HUNGER_DECLINE_RATE 2     // -2 every 30min
#define STATS_HAPPINESS_DECLINE_RATE 1  // -1 every 30min (base)
#define STATS_HEALTH_DECLINE_RATE 0     // Health doesn't decline automatically (affected by other stats)
#define STATS_FATIGUE_INCREASE_RATE 2   // +2 every 30min (fatigue increases)
#define STATS_HYGIENE_DECLINE_RATE 1    // -1 every 30min (base)

// --- Bonus de déclin quand il reste longtemps sans manger (faim basse) ---
// Plus la faim est basse, plus l'humeur et la propreté baissent en plus du déclin de base
#define STATS_HUNGER_THRESHOLD_LOW 60       // Faim < 60 : bonus -1 humeur, -1 propreté
#define STATS_HUNGER_THRESHOLD_CRITICAL 30  // Faim < 30 : bonus -2 humeur, -2 propreté
#define STATS_HAPPINESS_DECLINE_BONUS_LOW 1       // En plus quand faim < 60
#define STATS_HAPPINESS_DECLINE_BONUS_CRITICAL 2  // En plus quand faim < 30 (total = base + 2)
#define STATS_HYGIENE_DECLINE_BONUS_LOW 1
#define STATS_HYGIENE_DECLINE_BONUS_CRITICAL 2

// --- Intervalle de mise à jour des stats ---
#define STATS_UPDATE_INTERVAL_MS 1800000  // 30 minutes en millisecondes

// ============================================
// Configuration des actions NFC (objets)
// ============================================

// --- IDs des objets NFC ---
#define NFC_ITEM_BOTTLE "bottle"
#define NFC_ITEM_CAKE "cake"            // Gâteau
#define NFC_ITEM_CANDY "candy"         // Bonbon
#define NFC_ITEM_APPLE "apple"            // Pomme (fruit), variant 3

// --- Cooldowns (ms) ---
// Pas de cooldown sur la nourriture : quand il demande (faim), on peut lui donner tout de suite
#define NFC_BOTTLE_COOLDOWN_MS 0
#define NFC_CAKE_COOLDOWN_MS 0
#define NFC_CANDY_COOLDOWN_MS 0
#define NFC_APPLE_COOLDOWN_MS 0

#endif // MODEL_GOTCHI_CONFIG_H
