#include "vu_meter.h"
#include "pins.h"
#include "config.h"
#include <cmath>

void VUMeter::init() {
    strip = new Adafruit_NeoPixel(NUM_PIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
    strip->begin();
    strip->setBrightness(40);  // not blinding
    strip->clear();
    strip->show();
    Serial.println("[VU] Initialized");
}

void VUMeter::setBrightness(uint8_t b) {
    if (strip) strip->setBrightness(b);
}

uint32_t VUMeter::levelColor(int level, int maxLevel) {
    // Green → Yellow → Red gradient
    if (level < maxLevel / 3)      return strip->Color(0, 255, 0);
    if (level < maxLevel * 2 / 3)  return strip->Color(255, 255, 0);
    return strip->Color(255, 0, 0);
}

// ponytail: update() is currently dead code — no caller feeds it samples.
// Wire into audio pipeline (tap AudioOutputI2S buffer) when VU meter needed.
void VUMeter::update(const int16_t* samples, size_t count) {
    if (!strip || count < 2) return;

    // Calculate RMS for L and R channels
    float sumL = 0, sumR = 0;
    size_t frames = count / 2;
    for (size_t i = 0; i < frames; i++) {
        float l = (float)samples[i * 2];
        float r = (float)samples[i * 2 + 1];
        sumL += l * l;
        sumR += r * r;
    }
    float rmsL = sqrtf(sumL / frames) / 32768.0f;  // normalize to 0-1
    float rmsR = sqrtf(sumR / frames) / 32768.0f;

    // Peak hold with decay
    peakL = fmaxf(rmsL, peakL * decayRate);
    peakR = fmaxf(rmsR, peakR * decayRate);

    // Map to LEDs: 4 left (0-3), 4 right (4-7)
    int ledsL = (int)(peakL * 5.0f);  // 0-4, amplified a bit
    int ledsR = (int)(peakR * 5.0f);
    ledsL = min(ledsL, 4);
    ledsR = min(ledsR, 4);

    strip->clear();

    // Left channel: LEDs 3,2,1,0 (outside→inside)
    for (int i = 0; i < ledsL; i++) {
        strip->setPixelColor(3 - i, levelColor(i, 4));
    }

    // Right channel: LEDs 4,5,6,7 (inside→outside)
    for (int i = 0; i < ledsR; i++) {
        strip->setPixelColor(4 + i, levelColor(i, 4));
    }

    strip->show();
}

void VUMeter::off() {
    if (!strip) return;
    strip->clear();
    strip->show();
    peakL = 0;
    peakR = 0;
}