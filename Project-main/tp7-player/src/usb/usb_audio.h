#pragma once

namespace USBAudio {
    bool init();
    void stop();
    bool isActive();
    void loop();
}