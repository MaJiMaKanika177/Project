#pragma once
#include <cstdint>
#include <cstddef>
#include "config.h"

struct BiquadCoeffs {
    float a0, a1, a2, b1, b2;
};

class Equalizer {
public:
    void init(int sampleRate = SAMPLE_RATE);
    void setGain(int band, float dBGain);  // band 0-9, gain -12 to +12
    void process(int16_t* samples, size_t count);  // stereo interleaved
    float getGain(int band);
    void reset();
    bool isFlat();  // all gains ~0 → skip processing

    static constexpr int NUM_BANDS = EQ_BANDS;
    static const float CENTER_FREQS[EQ_BANDS];
    static const char* BAND_LABELS[EQ_BANDS];

private:
    int sr = SAMPLE_RATE;
    float gains[EQ_BANDS] = {};
    BiquadCoeffs coeffs[EQ_BANDS] = {};
    // Direct Form II transposed state, per channel
    float z1L[EQ_BANDS] = {}, z2L[EQ_BANDS] = {};
    float z1R[EQ_BANDS] = {}, z2R[EQ_BANDS] = {};
    void calcCoeffs(int band);
};