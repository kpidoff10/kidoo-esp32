# Gotchi — Waveshare ESP32-S3-Touch-AMOLED-1.75

## Matériel

- **MCU** : ESP32-S3R8 (8 Mo PSRAM OPI, 16 Mo flash externe)
- **Écran** : AMOLED 1,75″ 466×466, pilote CO5300 (QSPI)
- **Touch** : CST9217 (I2C)
- **Carte SD** : bus SPI (broches dans `config/config.h`)
- **Audio** : ES8311 + micros (ES7210), à intégrer au firmware Kidoo
- **RTC carte** : PCF85063 @ 0x51 — pris en charge dans `rtc_manager` (`KIDOO_RTC_PCF85063` dans `config.h`)
- **Doc** : [Waveshare Wiki](https://www.waveshare.net/wiki/ESP32-S3-Touch-AMOLED-1.75), [GitHub exemples](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75)

## Firmware

Environnement PlatformIO : `pio run -e gotchi`.

Prochaines étapes typiques : affichage (GFX/LVGL), touch, audio, driver PCF85063.
