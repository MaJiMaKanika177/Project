#pragma once

#define DEVICE_NAME        "TP7-Player"
#define FW_VERSION         "0.1.0"

// Audio
#define SAMPLE_RATE        44100
#define BITS_PER_SAMPLE    16
#define I2S_BUFFER_COUNT   8
#define I2S_BUFFER_SIZE    1024

// Display — GC9A01 round 240x240
#define SCREEN_WIDTH       240
#define SCREEN_HEIGHT      240
#define TFT_SPI_HZ         40000000

// Reel BLDC (gimbal motor) — set POLE_PAIRS from your motor spec.
// GBM2804 / 2208 gimbal motors are typically 7 pole pairs (14 magnets).
#define REEL_POLE_PAIRS    7
#define REEL_VOLTAGE_LIMIT 3.0f    // V — gimbal motors have tiny resistance
#define REEL_VOLTAGE_PSU   5.0f
#define REEL_PLAY_VEL      1.2f    // rad/s during playback
#define REEL_FFW_VEL       8.0f
// AS5600 addr/reg defined in reel_motor.cpp (0x36, 12-bit)

// NeoPixel
#define NUM_PIXELS         8

// EQ
#define EQ_BANDS           10

// WiFi AP fallback
#define AP_SSID            "TP7-Player"
#define AP_PASS            "tp7player"

// Web server
#define WEB_PORT           80