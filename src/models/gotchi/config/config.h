#ifndef CONFIG_GOTCHI_H
#define CONFIG_GOTCHI_H

#include <Arduino.h>

/**
 * Kidoo Gotchi — Waveshare ESP32-S3-Touch-AMOLED-1.75
 * 466×466 AMOLED (CO5300 QSPI), touch CST9217, PCF85063 RTC, ES8311 + ES7210,
 * SD SPI, QMI8658, AXP2101 (voir doc Waveshare).
 *
 * Broches alignées sur Mylibrary/pin_config.h du dépôt officiel :
 * https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75
 */

// ============================================
// Écran AMOLED CO5300 (QSPI) — pour intégration GFX / LVGL ultérieure
// ============================================

#define GOTCHI_LCD_SDIO0 4
#define GOTCHI_LCD_SDIO1 5
#define GOTCHI_LCD_SDIO2 6
#define GOTCHI_LCD_SDIO3 7
#define GOTCHI_LCD_SCLK 38
#define GOTCHI_LCD_CS 12
#define GOTCHI_LCD_RESET 39
#define GOTCHI_LCD_WIDTH 466
#define GOTCHI_LCD_HEIGHT 466

// ============================================
// Touch CST9217 (I2C)
// ============================================

#define GOTCHI_TP_I2C_SDA 15
#define GOTCHI_TP_I2C_SCL 14
#define GOTCHI_TP_INT 11
#define GOTCHI_TP_RESET 40

// ============================================
// I2C principal (touch, capteurs, PMU — même bus que les exemples Waveshare)
// ============================================

#define IIC_SDA 15
#define IIC_SCL 14

// ============================================
// RTC — PCF85063 (bus I2C partagé avec touch / capteurs Waveshare)
// rtc_manager : KIDOO_RTC_PCF85063 (registres NXP, adresse 7 bits 0x51)
// ============================================

#define RTC_SDA_PIN IIC_SDA
#define RTC_SCL_PIN IIC_SCL
#define KIDOO_RTC_PCF85063
#define RTC_I2C_ADDRESS 0x51

// ============================================
// Carte SD — mode SPI (SDManager) — tableau wiki Waveshare « 07_LVGL_SD_Test »
// ============================================

#define SD_MOSI_PIN 1
#define SD_MISO_PIN 3
#define SD_SCK_PIN 2
#define SD_CS_PIN 41

// ============================================
// Audio ES8311 / micros (référence Waveshare — intégration future)
// ============================================

#define GOTCHI_I2S_MCK_IO 16
#define GOTCHI_I2S_BCK_IO 9
#define GOTCHI_I2S_WS_IO 45
#define GOTCHI_I2S_DI_IO 10
#define GOTCHI_I2S_DO_IO 8

// ============================================
// BLE — bouton config (éviter GPIO 14/15 réservés au bus I2C de la carte)
// ============================================

#define BLE_CONFIG_BUTTON_PIN 17

// ============================================
// Bande WS2812 (optionnelle — requis pour compiler led_manager)
// GPIO 18 libre sur cette carte ; ajuster si bande externe branchée ailleurs.
// ============================================

#define LED_DATA_PIN 18
#define NUM_LEDS 16

#ifndef HAS_LED
#define HAS_LED false
#endif

// ============================================
// Composants logiques
// ============================================

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

#ifndef HAS_MQTT
#define HAS_MQTT true
#endif

#ifndef HAS_LVGL
#define HAS_LVGL 1
#endif

#endif // CONFIG_GOTCHI_H
