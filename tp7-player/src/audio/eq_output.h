#pragma once
#include <AudioOutput.h>
#include "eq.h"

// Wrapper that intercepts ConsumeSample() calls, buffers a block,
// applies EQ, then forwards to the real output.
// ESP8266Audio sends one stereo frame (2 samples) per ConsumeSample call.
class AudioOutputEQ : public AudioOutput {
public:
    AudioOutputEQ(AudioOutput* sink, Equalizer* eq)
        : realOut(sink), equalizer(eq) {}

    bool begin() override { return realOut->begin(); }
    bool stop()  override { return realOut->stop(); }

    bool SetRate(int hz)       override { return realOut->SetRate(hz); }
    bool SetBitsPerSample(int bits) override { return realOut->SetBitsPerSample(bits); }
    bool SetChannels(int ch)   override { return realOut->SetChannels(ch); }
    bool SetGain(float f)      override { return realOut->SetGain(f); }

    bool ConsumeSample(int16_t sample[2]) override {
        // Buffer frames, process in blocks for EQ efficiency
        buf[bufPos++] = sample[0];  // L
        buf[bufPos++] = sample[1];  // R

        if (bufPos >= BUF_FRAMES * 2) {
            if (equalizer && !equalizer->isFlat()) {
                equalizer->process(buf, bufPos);
            }
            // Flush to real output
            for (int i = 0; i < bufPos; i += 2) {
                int16_t frame[2] = { buf[i], buf[i + 1] };
                // Spin until output accepts (backpressure from I2S)
                while (!realOut->ConsumeSample(frame)) { yield(); }
            }
            bufPos = 0;
        }
        return true;
    }

private:
    AudioOutput* realOut;
    Equalizer* equalizer;
    static constexpr int BUF_FRAMES = 64;  // 64 stereo frames = 128 samples
    int16_t buf[BUF_FRAMES * 2] = {};
    int bufPos = 0;
};
