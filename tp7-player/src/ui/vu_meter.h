#pragma once
#include <Adafruit_NeoPixel.h>
#include <cstdint>
#include <cstddef>

class VUMeter {
public:
    void init();
    void update(const int16_t* samples, size_t count);  // stereo interleaved
    void off();
    void setBrightness(uint8_t b);

private:
    Adafruit_NeoPixel* strip = nullptr;
    float peakL = 0, peakR = 0;
    float decayRate = 0.85f;  // ponytail: fixed decay, configurable if needed

    uint32_t levelColor(int level, int maxLevel);
};