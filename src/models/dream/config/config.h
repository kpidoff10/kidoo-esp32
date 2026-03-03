#ifndef MODEL_DREAM_CONFIG_H
#define MODEL_DREAM_CONFIG_H

/**
 * Configuration du modèle Kidoo Dream (ESP32-C3 PRO Mini)
 *
 * Pinout physique (ESP32-C3 PRO Mini) :
 *   DROITE (5V, GND, 3V3) : GPIO4, GPIO3, GPIO2, GPIO1, GPIO0
 *   GAUCHE               : GPIO5, GPIO6, GPIO7, GPIO8, GPIO9, GPIO10, GPIO20, GPIO21
 *
 * Affectation des GPIO :
 *   DROITE : GPIO1=BLE, GPIO2=LED, GPIO4=SPI_SCK
 *   GAUCHE : GPIO5=MISO, GPIO6=MOSI, GPIO7=CS, GPIO8=SDA, GPIO9=SCL, GPIO10=Touch
 */

// ============================================
// I2C - Bus partagé (RTC DS3231, AHT20, BMP280)
// GAUCHE : GPIO 8 (SDA), 9 (SCL) = broches I2C matérielles du PRO Mini
// ============================================
#define RTC_SDA_PIN 8
#define RTC_SCL_PIN 9
#define RTC_I2C_ADDRESS 0x68

// ============================================
// SPI - Carte SD
// DROITE : GPIO4 (SCK) | GAUCHE : GPIO5,6,7 (MISO, MOSI, CS)
// ============================================
#define SD_SCK_PIN  4
#define SD_MISO_PIN 5
#define SD_MOSI_PIN 6
#define SD_CS_PIN   7

// ============================================
// LED WS2812B
// DROITE : GPIO 2 (libéré par I2C → SDA/SCL)
// ============================================
#define LED_DATA_PIN 2
#define NUM_LEDS 28
#define LED_TYPE NEOPIXEL
#define COLOR_ORDER GRB

// ============================================
// Entrées utilisateur
// DROITE : GPIO1 (BLE) | GAUCHE : GPIO10 (Touch)
// ============================================
#define BLE_CONFIG_BUTTON_PIN 1   // Appui long 3s = activation BLE
#define TOUCH_PIN 10              // Capteur tactile TTP223 (HIGH = touché)

// ============================================
// Composants disponibles sur ce modèle
// ============================================
// ESP32-C3 : Single-core, pas de PSRAM, GPIO limités
// On désactive les composants gourmands en ressources

#ifndef HAS_WIFI
#define HAS_WIFI true
#endif
#define HAS_SD_CARD true
#define HAS_LED true
#define HAS_BLE true           // Lazy init avec optimisations -Oz -flto
#ifndef HAS_RTC
#define HAS_RTC true
#endif
#define HAS_PUBNUB true
#define HAS_TOUCH true

// ============================================
// Capteur environnement AHT20 + BMP280 (I2C)
// Même bus I2C que le RTC : SDA = GPIO 8, SCL = GPIO 9
// AHT20 : 0x38 | BMP280 : 0x76 ou 0x77 (SDO à VCC)
// ============================================
#define HAS_ENV_SENSOR true
#ifndef ENV_BMP280_I2C_ADDR
#define ENV_BMP280_I2C_ADDR 0x76
#endif

#endif // MODEL_DREAM_CONFIG_H
