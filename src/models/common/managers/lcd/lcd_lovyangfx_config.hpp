#pragma once

#ifdef HAS_LCD

#include <LovyanGFX.hpp>
#include "../../../model_config.h"

/**
 * Configuration LovyanGFX pour ST7789 240x280 (Gotchi).
 * Pins définis dans gotchi/config/config.h
 */
class LGFX_Kidoo : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel_instance;
  lgfx::Bus_SPI _bus_instance;

public:
  LGFX_Kidoo(void) {
    {
      auto cfg = _bus_instance.config();
#if defined(ESP32) || defined(ESP32S3)
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 20000000;   // 20 MHz (comme Adafruit) pour stabilité avec bus partagé SD
      cfg.freq_read = 16000000;
      cfg.use_lock = true;         // Obligatoire pour partage SPI avec SD
      cfg.dma_channel = SPI_DMA_CH_AUTO;
#endif
      cfg.pin_sclk = TFT_SCK_PIN;
      cfg.pin_mosi = TFT_MOSI_PIN;
      cfg.pin_miso = SD_MISO_PIN;  // Requis pour bus partagé SD+LCD (SD lit sur MISO)
      cfg.pin_dc = TFT_DC_PIN;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs = TFT_CS_PIN;
      cfg.pin_rst = TFT_RST_PIN;
      cfg.pin_busy = -1;
      cfg.panel_width = TFT_WIDTH;
      cfg.panel_height = TFT_HEIGHT;
      cfg.offset_x = TFT_OFFSET_X;
      cfg.offset_y = TFT_OFFSET_Y;
      cfg.offset_rotation = (TFT_ROTATION & 3);
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = false;
      cfg.invert = true;  // Adafruit utilisait aussi invert sur ce type d'écran
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = true;  // Partagé avec SD (même MOSI/SCK)
      _panel_instance.config(cfg);
    }
    setPanel(&_panel_instance);
  }
};

#endif  // HAS_LCD
