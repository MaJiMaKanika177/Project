#pragma once
#include "screens.h"
#include "../audio/player.h"
#include "../audio/eq.h"
#include "../input/input_handler.h"

// Forward — UI doesn't own these, main passes pointers
class AudioPlayer;
class Equalizer;

struct UIState {
    Screen screen = Screen::SPLASH;
    int menuIndex = 0;         // cursor in file browser / settings
    int menuScroll = 0;        // scroll offset
    int eqBand = 0;            // selected EQ band
    int fileCount = 0;         // total files in current dir
    float batteryPct = -1;     // -1 = unknown
    bool btConnected = false;
    bool wifiConnected = false;
    uint32_t splashEnd = 0;    // millis when splash should end
    int scrollOffset = 0;      // text scroll for long track names
    uint32_t lastScrollTime = 0;
};

namespace UI {
    void init(AudioPlayer* player, Equalizer* eq);
    void handleInput(const InputMsg& msg);
    void render();  // call ~30fps
    void taskFunc(void* param);  // FreeRTOS task entry

    UIState& getState();
    void setScreen(Screen s);
    void setBattery(float pct);
    void setBtConnected(bool c);
    void setWifiConnected(bool c);
}