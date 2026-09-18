#pragma once
#include <Arduino.h>

namespace WiFiMgr {
    void init();
    void loop();    // call periodically — processes captive portal DNS
    bool connectSTA(const char* ssid, const char* pass, uint32_t timeoutMs = 10000);
    void startAP();
    void stop();
    bool isConnected();
    bool isAP();
    String getIP();
    String scanNetworksJSON();  // returns JSON array
}