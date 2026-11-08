#include "Boards/Board.h"
#include "../../Hardware/BoardIO.h"

#include <driver/gpio.h>
#include <esp_log.h>
#include "sdkconfig.h"

// USB-OTG nativo usa GPIO19/20, que nesta placa estao livres, mas o conector
// da placa e so o UART (CH340), entao o TinyUSB nao serve para nada aqui.
#if CONFIG_CELEROS_USB_NATIVE
#error "CONFIG_CELEROS_USB_NATIVE nao faz sentido na ES3C28P (conector e so UART)"
#endif

// Perfil da ES3C28P (lcdwiki 2.8inch_ESP32-S3_Display): ESP32-S3-WROOM-1
// N16R8, ILI9341 240x320 em SPI, touch capacitivo FT6336G (I2C), SD, codec
// de audio com amplificador e LED RGB WS2812.

namespace {
BoardDisplay s_display;
}  // namespace

namespace Board {

static const BoardProfile s_profile = {
    .id = "es3c28p",
    .otaChannel = "es3c28p",
    .name = "es3c28p (ESP32-S3 2.8\" ILI9341)",
    .sd = {.cs = -1, .sck = -1, .miso = -1, .mosi = -1, .freqKhz = 20000, .spiHost = -1,
           .sdmmc = true, .clk = 38, .cmd = 40, .d0 = 39, .d1 = 41, .d2 = 48, .d3 = 47},
    .hasPsram = true,
    .backlightPwm = true,
    .capacitiveTouch = true,
    .speakerPin = -1,
    .rotation = 0,     // CONFERIR: 240x320 retrato; ajuste se o app esperar paisagem
    .led = {42, -1, -1, false},  // WS2812 em GPIO42
    .lightSensorPin = -1,
    .i2s = {.dout = 6, .bclk = 5, .lrc = 7},  // CONFERIR: codec com MCLK=4, DIN=8, PA_EN=1
    .relay = {},
    .servo = {},
    .mic = {},
    .strips = {},
    .i2c = {.port = 0, .sda = 16, .scl = 15, .hz = 400000},  // FT6336G (RST=18, INT=17)
    .watchGestures = true,
    // Reservados (nao viram GPIO de app): LCD SPI (10/11/12/13/46), backlight
    // (45), touch (15/16/17/18), SD (38/39/40), audio (1/4/5/6/7/8), LED (42),
    // UART0 (43/44) e PSRAM octal (26..37).
    .gpioDeniedMask = (1ULL << 1) | (1ULL << 4) | (1ULL << 5) | (1ULL << 6) |
                      (1ULL << 7) | (1ULL << 8) | (1ULL << 10) | (1ULL << 11) |
                      (1ULL << 12) | (1ULL << 13) | (1ULL << 15) | (1ULL << 16) |
                      (1ULL << 17) | (1ULL << 18) | (1ULL << 38) | (1ULL << 39) |
                      (1ULL << 40) | (1ULL << 41) | (1ULL << 42) | (1ULL << 47) | (1ULL << 48) | (1ULL << 43) | (1ULL << 44) |
                      (1ULL << 45) | (1ULL << 46) | (0xFFFULL << 26) ,
};

void init() {
    // Backlight/CS do painel ficam a cargo do init do LovyanGFX (Panel_ILI9341
    // + Bus_SPI na struct BoardDisplay).
    s_display.init();
    // Arrays RGB565 padrao (icones, BMPs de apps JS) precisam da troca de bytes.
    s_display.setSwapBytes(true);
}

CelerDisplay& display() {
    return s_display;
}

const BoardProfile& profile() {
    return s_profile;
}

}  // namespace Board