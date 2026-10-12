#include "eq.h"
#include "../system/settings.h"
#include <cmath>
#include <cstring>

const float Equalizer::CENTER_FREQS[EQ_BANDS] = {
    31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000
};

const char* Equalizer::BAND_LABELS[EQ_BANDS] = {
    "31", "62", "125", "250", "500", "1K", "2K", "4K", "8K", "16K"
};

void Equalizer::init(int sampleRate) {
    sr = sampleRate;
    reset();
    for (int i = 0; i < EQ_BANDS; i++) calcCoeffs(i);
}

void Equalizer::reset() {
    memset(gains, 0, sizeof(gains));
    memset(z1L, 0, sizeof(z1L));
    memset(z2L, 0, sizeof(z2L));
    memset(z1R, 0, sizeof(z1R));
    memset(z2R, 0, sizeof(z2R));
    for (int i = 0; i < EQ_BANDS; i++) calcCoeffs(i);
}

void Equalizer::setGain(int band, float dBGain) {
    if (band < 0 || band >= EQ_BANDS) return;
    dBGain = fmaxf(-12.0f, fminf(12.0f, dBGain));
    gains[band] = dBGain;
    calcCoeffs(band);
    Settings::markDirty();
}

float Equalizer::getGain(int band) {
    return (band >= 0 && band < EQ_BANDS) ? gains[band] : 0;
}

bool Equalizer::isFlat() {
    for (int i = 0; i < EQ_BANDS; i++) {
        if (fabsf(gains[i]) > 0.1f) return false;
    }
    return true;
}

void Equalizer::calcCoeffs(int band) {
    float f0 = CENTER_FREQS[band];
    float Q = 1.41f;  // ponytail: fixed Q, per-band Q if user wants parametric
    float A = powf(10.0f, gains[band] / 40.0f);
    float w0 = 2.0f * M_PI * f0 / sr;
    float sinW0 = sinf(w0);
    float cosW0 = cosf(w0);
    float alpha = sinW0 / (2.0f * Q);

    float b0 =  1.0f + alpha * A;
    float b1 = -2.0f * cosW0;
    float b2 =  1.0f - alpha * A;
    float a0 =  1.0f + alpha / A;
    float a1 = -2.0f * cosW0;
    float a2 =  1.0f - alpha / A;

    // Normalize
    coeffs[band] = { b0/a0, b1/a0, b2/a0, a1/a0, a2/a0 };
}

void Equalizer::process(int16_t* samples, size_t count) {
    if (isFlat()) return;  // skip if all bands flat

    size_t frames = count / 2;  // stereo interleaved
    for (int b = 0; b < EQ_BANDS; b++) {
        if (fabsf(gains[b]) < 0.1f) continue;  // skip inactive bands
        auto& c = coeffs[b];

        for (size_t i = 0; i < frames; i++) {
            // Left channel
            float inL = (float)samples[i * 2];
            float outL = c.a0 * inL + z1L[b];
            z1L[b] = c.a1 * inL - c.b1 * outL + z2L[b];
            z2L[b] = c.a2 * inL - c.b2 * outL;
            samples[i * 2] = (int16_t)fmaxf(-32768.0f, fminf(32767.0f, outL));

            // Right channel
            float inR = (float)samples[i * 2 + 1];
            float outR = c.a0 * inR + z1R[b];
            z1R[b] = c.a1 * inR - c.b1 * outR + z2R[b];
            z2R[b] = c.a2 * inR - c.b2 * outR;
            samples[i * 2 + 1] = (int16_t)fmaxf(-32768.0f, fminf(32767.0f, outR));
        }
    }
}