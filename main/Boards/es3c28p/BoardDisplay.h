#ifndef CELER_BOARDS_ES3C28P_DISPLAY_H
#define CELER_BOARDS_ES3C28P_DISPLAY_H

// ----------------------------------------------------------------------------
// ES3C28P (lcdwiki 2.8inch_ESP32-S3_Display)
//
// Painel 2.8" 240x320, ILI9341 em SPI (SPI2_HOST): SCLK=12, MOSI=11, MISO=13,
// DC=46, CS=10, RST=-1 (ligado ao EN), backlight em GPIO45 (PWM). Touch
// capacitivo FT6336G (I2C SDA=16, SCL=15, RST=18, INT=17, addr 0x38) — driver
// i2c_master oficial (mesmo motivo do GT911: o LL do LovyanGFX falha no IDF 6.1).
// ----------------------------------------------------------------------------

#include "../../Display/Display.h"

#include "../../Display/Touch_FT6336_IDF.h"   // a criar (copia do Touch_GT911_IDF.h)
#include <lgfx/v1/platforms/esp32/Bus_SPI.hpp>
#include <lgfx/v1/panel/Panel_ILI9341.hpp>

class BoardDisplay : public CelerDisplayBase {
public:
    lgfx::Bus_SPI         _bus;
    lgfx::Panel_ILI9341   _panel;
    lgfx::Light_PWM       _light;
    lgfx::Touch_FT6336_IDF _touch;

    BoardDisplay(void) {
        {
            auto cfg = _bus.config();
            cfg.spi_host    = SPI2_HOST;
            cfg.spi_mode    = 0;
            cfg.freq_write  = 40000000;
            cfg.freq_read   = 16000000;
            cfg.spi_3wire   = false;
            cfg.use_lock    = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk    = 12;
            cfg.pin_mosi    = 11;
            cfg.pin_miso    = 13;
            cfg.pin_dc      = 46;
            _bus.config(cfg);
            _panel.setBus(&_bus);
        }
        {
            auto cfg = _panel.config();
            cfg.pin_cs           = 10;
            cfg.pin_rst          = -1;
            cfg.pin_busy         = -1;
            cfg.panel_width      = 240;
            cfg.panel_height     = 320;
            cfg.memory_width     = 240;
            cfg.memory_height    = 320;
            cfg.offset_x         = 0;
            cfg.offset_y         = 0;
            cfg.offset_rotation  = 2;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;
            cfg.readable         = true;
            cfg.invert           = true;  // CONFERIR: se as cores sairem invertidas, true
            cfg.rgb_order        = false;  // CONFERIR: se vermelho/azul trocados, true
            cfg.dlen_16bit       = false;
            cfg.bus_shared       = false;
            _panel.config(cfg);
        }
        {
            auto cfg = _light.config();
            cfg.pin_bl      = 45;
            cfg.invert      = false;
            cfg.freq        = 44100;
            cfg.pwm_channel = 7;
            _light.config(cfg);
            _panel.light(&_light);
        }
        {
            auto cfg = _touch.config();
            cfg.x_min = 0;
            cfg.x_max = 239;
            cfg.y_min = 0;
            cfg.y_max = 319;
            cfg.pin_int = 17;
            cfg.pin_rst = 18;
            cfg.bus_shared = false;
            cfg.offset_rotation = 2;
            cfg.pin_sda = 16;
            cfg.pin_scl = 15;
            cfg.freq = 400000;
            cfg.i2c_addr = 0x38;
            _touch.config(cfg);
            _panel.setTouch(&_touch);
        }

        setPanel(&_panel);
    }

    // Touch capacitivo: calibracao do XPT2046 nao se aplica (base = no-op)
};

#endif  // CELER_BOARDS_ES3C28P_DISPLAY_H