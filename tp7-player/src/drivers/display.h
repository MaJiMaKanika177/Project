#pragma once
#include <LovyanGFX.hpp>

// GC9A01 240x240 round IPS on its own SPI bus (SD keeps SPI2 to itself).
class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_GC9A01  _panel;
    lgfx::Bus_SPI       _bus;
    lgfx::Light_PWM     _light;
public:
    LGFX();
};

namespace Display {
    bool init();
    LGFX& get();
    // Off-screen 240x240 sprite in PSRAM — all UI draws here, then one push.
    LGFX_Sprite& canvas();
    void flush();            // push canvas to panel
    void splash();
    void clear();
    void setBrightness(uint8_t b);  // 0-255

    static constexpr int W = 240;
    static constexpr int H = 240;
    static constexpr int CX = 120;
    static constexpr int CY = 120;
    static constexpr int R  = 120;
}