#pragma once
#include <AudioFileSourceHTTPStream.h>
#include <AudioFileSourceBuffer.h>
#include <AudioGeneratorMP3.h>
#include <AudioGeneratorAAC.h>
#include <AudioOutputI2S.h>

class HttpPlayer {
public:
    bool init(AudioOutputI2S* output);
    bool play(const char* url);
    void stop();
    void loop();
    bool isPlaying();
    const char* getStreamUrl();

private:
    AudioFileSourceHTTPStream* http = nullptr;
    AudioFileSourceBuffer* buf = nullptr;
    AudioGenerator* gen = nullptr;
    AudioOutputI2S* out = nullptr;
    bool playing = false;
    char streamUrl[512] = {};
    void cleanup();
};