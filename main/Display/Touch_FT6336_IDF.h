#ifndef CELEROS_TOUCH_FT6336_IDF_H
#define CELEROS_TOUCH_FT6336_IDF_H

// Touch FT6336G sobre o driver i2c_master oficial do ESP-IDF (mesmo motivo do
// GT911: o LL de I2C do LovyanGFX falha no IDF 6.1). Implementa ITouch.
//
// Protocolo (addr 0x38), lido de uma vez a partir do reg 0x02:
//   [0x02] TD_STATUS: bits 3..0 = numero de toques (0..2)
//   [0x03..0x08] ponto 1, [0x09..0x0E] ponto 2, 6 bytes cada:
//     [XH: evento(7:6) | x(11:8)] [XL] [YH: id(7:4) | y(11:8)] [YL] [peso] [area]
// Nao ha flag para limpar (diferente do GT911): e so ler. Leitura em SEGUNDO
// PLANO (task "ft6336", nucleo 0); getTouchRaw so copia o ultimo estado.

#include "lgfx/v1/Touch.hpp"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include <string.h>

namespace lgfx
{
 inline namespace v1
 {
//----------------------------------------------------------------------------

  struct Touch_FT6336_IDF : public ITouch
  {
    static constexpr const uint8_t default_addr = 0x38;

    Touch_FT6336_IDF(void)
    {
      _cfg.i2c_addr = default_addr;
      _cfg.freq = 400000;
    }

    ~Touch_FT6336_IDF(void) override
    {
      deinit();
    }

    bool init(void) override
    {
      if (_inited) return true;
      deinit();

      // reset por hardware (a placa liga RST do touch em GPIO18)
      if (_cfg.pin_rst >= 0) {
        gpio_reset_pin((gpio_num_t)_cfg.pin_rst);
        gpio_set_direction((gpio_num_t)_cfg.pin_rst, GPIO_MODE_OUTPUT);
        gpio_set_level((gpio_num_t)_cfg.pin_rst, 0);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level((gpio_num_t)_cfg.pin_rst, 1);
        vTaskDelay(pdMS_TO_TICKS(300));  // o chip leva ~300 ms para subir
      }

      for (int retry = 6; retry; --retry)
      {
        if (!openBus()) { deinit(); return false; }

        // Valida lendo o vendor id (0xA8) e o chip id (0xA3)
        uint8_t reg_vendor[1] = {0xA8};
        uint8_t reg_chip[1]   = {0xA3};
        uint8_t vendor = 0, chip = 0;
        if (writeRead(reg_vendor, 1, &vendor, 1)) {
            writeRead(reg_chip, 1, &chip, 1);
            _inited = true;
            ESP_LOGI("celer.touch", "FT6336 init OK (addr 0x%02X vendor=0x%02X chip=0x%02X)",
                     (unsigned)_cfg.i2c_addr, vendor, chip);
            startPoller();
            return true;
        }
        closeBus();
        vTaskDelay(pdMS_TO_TICKS(20));
      }
      ESP_LOGE("celer.touch", "FT6336 nao respondeu em 0x%02X", (unsigned)_cfg.i2c_addr);
      return false;
    }

    void wakeup(void) override {}
    void sleep(void) override {}

    uint_fast8_t getTouchRaw(touch_point_t* tp, uint_fast8_t count) override
    {
      if (!_inited || tp == nullptr || count == 0) return 0;
      if (count > 2) count = 2;

      if (_task != nullptr) {
        uint8_t snap[sizeof(_readdata)];
        portENTER_CRITICAL(&_mux);
        memcpy(snap, _shared, sizeof(snap));
        portEXIT_CRITICAL(&_mux);
        return parse(snap, tp, count);
      }
      poll();
      return parse(_readdata, tp, count);
    }

  private:
    static constexpr uint32_t kActiveMs = 10;  // com dedo: arrasto sem perder amostras
    static constexpr uint32_t kIdleMs   = 20;  // parado: INT nao e usado, so polling

    void poll(void)
    {
      uint32_t msec = lgfx::millis();
      const uint32_t interval = (_readdata[0] & 0x0F) ? kActiveMs : kIdleMs;
      if (msec - _last_update >= interval) {
        _last_update = msec;
        updateData();
      }
    }

    // _rd[0] = numero de toques; depois 2 x 6 bytes crus dos pontos
    uint_fast8_t parse(const uint8_t* rd, touch_point_t* tp, uint_fast8_t count)
    {
      uint32_t points = rd[0] & 0x0F;
      if (points > 2) points = 0;  // valor invalido (ex.: 0x0F durante transicao)
      if (points > count) points = count;
      for (size_t idx = 0; idx < points; ++idx)
      {
        const uint8_t* b = &rd[1 + idx * 6];
        tp[idx].id   = b[2] >> 4;
        tp[idx].x    = (uint16_t)(((b[0] & 0x0F) << 8) | b[1]);
        tp[idx].y    = (uint16_t)(((b[2] & 0x0F) << 8) | b[3]);
        tp[idx].size = b[4];

        if (tp[idx].x < _cfg.x_min) tp[idx].x = _cfg.x_min;
        if (tp[idx].x > _cfg.x_max) tp[idx].x = _cfg.x_max;
        if (tp[idx].y < _cfg.y_min) tp[idx].y = _cfg.y_min;
        if (tp[idx].y > _cfg.y_max) tp[idx].y = _cfg.y_max;
      }
      return points;
    }

    bool updateData(void)
    {
      static constexpr uint8_t reg[1] = {0x02};
      uint8_t buf[13] = {};  // 0x02 (status) + 2 pontos x 6 bytes
      if (!writeRead(reg, 1, buf, sizeof(buf))) return false;
      _readdata[0] = buf[0] & 0x0F;
      memcpy(&_readdata[1], &buf[1], 12);
      return true;
    }

    static void pollTask(void* arg)
    {
      auto* self = static_cast<Touch_FT6336_IDF*>(arg);
      for (;;) {
        self->poll();
        portENTER_CRITICAL(&self->_mux);
        memcpy(self->_shared, self->_readdata, sizeof(self->_shared));
        portEXIT_CRITICAL(&self->_mux);
        const uint32_t iv = (self->_readdata[0] & 0x0F) ? kActiveMs : kIdleMs;
        vTaskDelay(pdMS_TO_TICKS(iv));
      }
    }

    void startPoller(void)
    {
      if (_task != nullptr) return;
      BaseType_t ok = xTaskCreatePinnedToCoreWithCaps(pollTask, "ft6336", 3072, this, 2, &_task, 0,
                                                      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      if (ok != pdPASS) ok = xTaskCreatePinnedToCore(pollTask, "ft6336", 3072, this, 2, &_task, 0);
      if (ok != pdPASS) _task = nullptr;   // segue sincrono
    }

    TaskHandle_t _task = nullptr;
    portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
    uint8_t _shared[13] = {};

    i2c_master_bus_handle_t _bus = nullptr;
    i2c_master_dev_handle_t _dev = nullptr;
    bool _inited = false;
    uint32_t _last_update = 0;
    uint8_t _readdata[13] = {};  // [0]=toques; pontos a partir de [1]

    bool openBus(void)
    {
        i2c_master_bus_config_t bcfg = {};
        bcfg.i2c_port = _cfg.i2c_port;
        bcfg.sda_io_num = (gpio_num_t)_cfg.pin_sda;
        bcfg.scl_io_num = (gpio_num_t)_cfg.pin_scl;
        bcfg.clk_source = I2C_CLK_SRC_DEFAULT;
        bcfg.glitch_ignore_cnt = 7;
        bcfg.flags.enable_internal_pullup = true;
        if (i2c_new_master_bus(&bcfg, &_bus) != ESP_OK) {
            _bus = nullptr;
            return false;
        }

        i2c_device_config_t dcfg = {};
        dcfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        dcfg.device_address = (uint16_t)_cfg.i2c_addr;
        uint32_t freq = _cfg.freq;
        if (freq == 0 || freq > 400000) freq = 400000;  // FT6336: max 400 kHz
        dcfg.scl_speed_hz = freq;
        if (i2c_master_bus_add_device(_bus, &dcfg, &_dev) != ESP_OK) {
            closeBus();
            return false;
        }
        return true;
    }

    void closeBus(void)
    {
        if (_dev) { i2c_master_bus_rm_device(_dev); _dev = nullptr; }
        if (_bus) { i2c_del_master_bus(_bus); _bus = nullptr; }
    }

    void deinit(void)
    {
        closeBus();
        _inited = false;
    }

    bool writeRead(const uint8_t* w, size_t wl, uint8_t* r, size_t rl)
    {
        return i2c_master_transmit_receive(_dev, w, wl, r, rl, 20) == ESP_OK;
    }
};

//----------------------------------------------------------------------------
 }
}

#endif // CELEROS_TOUCH_FT6336_IDF_H