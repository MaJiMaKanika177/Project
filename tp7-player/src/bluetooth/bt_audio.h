#pragma once
#include <cstdint>

namespace BTAudio {
    enum class Mode : uint8_t { OFF, SINK, SOURCE };

    void initSink(const char* deviceName = "TP7-Player");
    void initSource(const char* targetName);
    void disconnect();
    void stop();
    bool isConnected();
    Mode getMode();
    const char* getTrackTitle();

    typedef void (*VolumeCallback)(uint8_t vol);
    void setVolumeCallback(VolumeCallback cb);
}