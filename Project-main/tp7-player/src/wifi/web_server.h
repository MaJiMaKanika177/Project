#pragma once
#include <ESPAsyncWebServer.h>

// Forward declarations — main.cpp sets these
class AudioPlayer;
class Equalizer;

namespace WebServer {
    void init();
    void setPlayerRef(AudioPlayer* p);
    void setEqRef(Equalizer* e);
    AsyncWebServer& getServer();
}