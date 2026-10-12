#pragma once
#include <AudioFileSourceSD.h>
#include <AudioGeneratorMP3.h>
#include <AudioGeneratorWAV.h>
#include <AudioGeneratorFLAC.h>
#include <AudioOutputI2S.h>
#include <AudioFileSourceBuffer.h>
#include <freertos/semphr.h>
#include "util.h"

class AudioOutputEQ;  // forward decl
class Equalizer;      // forward decl

enum class PlayState { STOPPED, PLAYING, PAUSED };
// AudioFormat moved to include/util.h

class AudioPlayer {
public:
    bool init(Equalizer* eq = nullptr);
    bool play(const char* path);
    void stop();
    void pause();
    void resume();
    void loop();  // call from FreeRTOS task
    bool isPlaying();
    PlayState getState();
    float getProgress();       // 0.0 - 1.0
    void setVolume(float vol); // 0.0 - 1.0
    float getVolume();
    const char* getCurrentTrack();

private:
    AudioFileSourceSD* src = nullptr;
    AudioFileSourceBuffer* buf = nullptr;
    AudioGenerator* gen = nullptr;
    AudioOutputI2S* out = nullptr;
    PlayState state = PlayState::STOPPED;
    float volume = 0.5f;
    char currentTrack[256] = {};
    SemaphoreHandle_t mtx = nullptr;

    AudioFormat detectFormat(const char* path);
    void cleanup();
    Equalizer* eqRef = nullptr;
    AudioOutputEQ* eqOut = nullptr;

    // Lock helpers
    void lock()   { if (mtx) xSemaphoreTake(mtx, portMAX_DELAY); }
    void unlock() { if (mtx) xSemaphoreGive(mtx); }
};