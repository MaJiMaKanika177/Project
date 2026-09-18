#include "display.h"
#include "pins.h"
#include "config.h"
#include <Arduino.h>

LGFX::LGFX() {
    {   auto cfg = _bus.config();
        cfg.spi_host   = SPI3_HOST;      // SPI2 is the SD card
        cfg.spi_mode   = 0;
        cfg.freq_write = TFT_SPI_HZ;
        cfg.freq_read  = 16000000;
        cfg.spi_3wire  = false;
        cfg.use_lock   = true;
        cfg.dma_channel = SPI_DMA_CH_AUTO;
        cfg.pin_sclk   = PIN_TFT_SCK;
        cfg.pin_mosi   = PIN_TFT_MOSI;
        cfg.pin_miso   = -1;
        cfg.pin_dc     = PIN_TFT_DC;
        _bus.config(cfg);
        _panel.setBus(&_bus);
    }
    {   auto cfg = _panel.config();
        cfg.pin_cs   = PIN_TFT_CS;
        cfg.pin_rst  = PIN_TFT_RST;
        cfg.pin_busy = -1;
        cfg.panel_width  = SCREEN_WIDTH;
        cfg.panel_height = SCREEN_HEIGHT;
        cfg.offset_x = 0;
        cfg.offset_y = 0;
        cfg.offset_rotation = 0;
        cfg.readable = false;
        cfg.invert   = true;       // GC9A01 ships inverted
        cfg.rgb_order = false;
        cfg.dlen_16bit = false;
        cfg.bus_shared = false;    // dedicated bus, no SD contention
        _panel.config(cfg);
    }
    {   auto cfg = _light.config();
        cfg.pin_bl = PIN_TFT_BL;
        cfg.invert = false;
        cfg.freq   = 12000;
        cfg.pwm_channel = 7;
        _light.config(cfg);
        _panel.setLight(&_light);
    }
    setPanel(&_panel);
}

static LGFX tft;
static LGFX_Sprite cv(&tft);
static bool ready = false;

bool Display::init() {
    if (!tft.init()) {
        Serial.println("[Display] GC9A01 init failed");
        return false;
    }
    tft.setRotation(0);
    tft.setBrightness(200);
    tft.fillScreen(TFT_BLACK);

    cv.setPsram(true);                 // 240*240*2 = 115KB — PSRAM, not SRAM
    cv.setColorDepth(16);
    if (!cv.createSprite(W, H)) {
        Serial.println("[Display] canvas alloc failed");
        return false;
    }
    ready = true;
    splash();
    return true;
}

LGFX& Display::get() { return tft; }
LGFX_Sprite& Display::canvas() { return cv; }

void Display::flush() {
    if (ready) cv.pushSprite(0, 0);
}

void Display::setBrightness(uint8_t b) { tft.setBrightness(b); }

void Display::splash() {
    if (!ready) return;
    cv.fillScreen(TFT_BLACK);
    cv.setTextDatum(middle_center);
    cv.setTextColor(TFT_WHITE);
    cv.setFont(&fonts::FreeSansBold18pt7b);
    cv.drawString("TP-7", CX, CY - 14);
    cv.setFont(&fonts::Font2);
    cv.setTextColor(0x8410);           // grey
    cv.drawString(FW_VERSION, CX, CY + 20);
    cv.drawCircle(CX, CY, R - 2, 0x39C7);
    flush();
}

void Display::clear() {
    if (!ready) return;
    cv.fillScreen(TFT_BLACK);
    flush();
}