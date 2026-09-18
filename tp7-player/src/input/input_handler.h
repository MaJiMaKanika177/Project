#pragma once
#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

enum class Button : uint8_t {
    PLAY = 0, MEMO, MENU, BACK, ROCKER_UP, ROCKER_DN, COUNT
};

enum class InputEvent : uint8_t {
    NONE = 0,
    // Button events
    BTN_PRESS,
    BTN_LONG_PRESS,   // held > 600ms
    BTN_RELEASE,
    // Encoder events
    ENC_CW,           // clockwise
    ENC_CCW,          // counter-clockwise
    ENC_PRESS,        // encoder push button
    ENC_LONG_PRESS    // encoder held > 600ms
};

struct InputMsg {
    InputEvent event;
    Button button;       // valid for BTN_* events
    int32_t encoderDelta; // valid for ENC_CW/CCW — accumulated ticks
};

namespace Input {
    void init();
    void taskFunc(void* param);  // FreeRTOS task entry

    // Non-blocking read from event queue. Returns true if event available.
    bool poll(InputMsg& msg);

    // Get queue handle (for tasks that want to block on xQueueReceive directly)
    QueueHandle_t getQueue();
}