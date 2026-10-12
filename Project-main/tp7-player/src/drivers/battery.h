#pragma once

namespace Battery {
    bool init();
    float percent();     // 0-100, -1 if unavailable
    float voltage();     // volts
    bool isLow();        // < 10%
    void poll();         // call periodically (30s)
}