#pragma once

// Battery ADC
#define PIN_BAT_ADC   18   // Available analog pin
#define BAT_R1        100000.0f // 100k
#define BAT_R2        100000.0f // 100k

// I2S — PCM5102A
#define PIN_I2S_BCK   4
#define PIN_I2S_WS    5
#define PIN_I2S_DOUT  6

// SPI — SD Card
#define PIN_SD_SCK   10
#define PIN_SD_MOSI  11
#define PIN_SD_MISO  12
#define PIN_SD_CS    13

// I2C bus 0 (Wire) — OLED-free now; MAX17048 (0x36), MCP23017 (0x20)
#define PIN_I2C_SDA   8
#define PIN_I2C_SCL   9

// I2C bus 1 (Wire1) — AS5600 angle sensor (0x36)
// Separate bus keeps the 400kHz sensor poll off the button/MCP bus.
#define PIN_ENC_SDA   1
#define PIN_ENC_SCL   2

// Reel push-button (physical switch under the reel disc)
#define PIN_ENC_SW    3

// BLDC gimbal motor — DRV8313 / SimpleFOC Mini (3-phase)
#define PIN_BLDC_IN1 14
#define PIN_BLDC_IN2 15
#define PIN_BLDC_IN3 16
#define PIN_BLDC_EN  21

// Round TFT — GC9A01 240x240, own SPI bus (SD keeps bus 0 at low clock)
#define PIN_TFT_SCK  42
#define PIN_TFT_MOSI 41
#define PIN_TFT_CS   38
#define PIN_TFT_DC   39
#define PIN_TFT_RST  40
#define PIN_TFT_BL   47

// NeoPixel
#define PIN_NEOPIXEL 48

// USB Native — do not reuse
#define PIN_USB_DN    19
#define PIN_USB_DP    20

// MCP23017 interrupt
#define PIN_MCP_INT   7

// MCP23017 pin assignments (GPA0-7)
#define MCP_BTN_PLAY   0
#define MCP_BTN_MEMO   1
#define MCP_BTN_MENU   2
#define MCP_BTN_BACK   3
#define MCP_BTN_ROCKER_UP   4
#define MCP_BTN_ROCKER_DN   5
#define MCP_LED_RED    6
// GPA7 = spare

// Reserved by hardware on ESP32-S3 with octal PSRAM (qio_opi): GPIO26-37.
// Free after this map: 0 (strapping), 17, 18, 43, 44, 45/46 (strapping).