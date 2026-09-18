#pragma once
#include <driver/i2s.h>

namespace I2SOut {
    bool init(int sample_rate = 44100, int bits = 16);
    void write(const int16_t* samples, size_t count);
    void stop();
}