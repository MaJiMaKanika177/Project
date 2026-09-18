#include "input_handler.h"
#include "pins.h"
#include "../drivers/reel_motor.h"
#include <Arduino.h>
#include <Adafruit_MCP23X17.h>

// ── State ──────────────────────────────────────────────
static QueueHandle_t eventQueue = nullptr;
static Adafruit_MCP23X17 mcp;
static volatile bool mcpIntFlag = false;

struct BtnState {
    bool pressed;
    uint32_t pressTime;
    bool longFired;
};
static BtnState btnStates[(int)Button::COUNT] = {};

// ── ISRs ───────────────────────────────────────────────
static void IRAM_ATTR mcpISR() {
    mcpIntFlag = true;
}

// ── MCP23017 button scan ───────────────────────────────
static const uint8_t MCP_PINS[] = {
    MCP_BTN_PLAY, MCP_BTN_MEMO, MCP_BTN_MENU,
    MCP_BTN_BACK, MCP_BTN_ROCKER_UP, MCP_BTN_ROCKER_DN
};
static const int NUM_BTNS = sizeof(MCP_PINS) / sizeof(MCP_PINS[0]);

static void sendEvent(InputEvent evt, Button btn = Button::PLAY, int32_t delta = 0) {
    InputMsg msg = { evt, btn, delta };
    xQueueSend(eventQueue, &msg, 0);  // non-blocking
}

static void scanButtons() {
    uint32_t now = millis();

    for (int i = 0; i < NUM_BTNS; i++) {
        bool cur = !mcp.digitalRead(MCP_PINS[i]);  // active LOW (pulled up)
        BtnState& s = btnStates[i];
        Button btn = (Button)i;

        if (cur && !s.pressed) {
            s.pressed = true;
            s.pressTime = now;
            s.longFired = false;
            sendEvent(InputEvent::BTN_PRESS, btn);
        }
        else if (cur && s.pressed && !s.longFired) {
            if (now - s.pressTime > 600) {
                s.longFired = true;
                sendEvent(InputEvent::BTN_LONG_PRESS, btn);
            }
        }
        else if (!cur && s.pressed) {
            s.pressed = false;
            sendEvent(InputEvent::BTN_RELEASE, btn);
        }
    }
}

// ── Reel push button (direct GPIO) ─────────────────────
static BtnState reelBtnState = {};

static void scanReelButton() {
    uint32_t now = millis();
    bool cur = !digitalRead(PIN_ENC_SW);  // active LOW

    if (cur && !reelBtnState.pressed) {
        reelBtnState.pressed = true;
        reelBtnState.pressTime = now;
        reelBtnState.longFired = false;
        sendEvent(InputEvent::ENC_PRESS);
    }
    else if (cur && reelBtnState.pressed && !reelBtnState.longFired) {
        if (now - reelBtnState.pressTime > 600) {
            reelBtnState.longFired = true;
            sendEvent(InputEvent::ENC_LONG_PRESS);
        }
    }
    else if (!cur && reelBtnState.pressed) {
        reelBtnState.pressed = false;
    }
}

// ── Reel rotation → detent events ──────────────────────
// The BLDC's FOC task owns the angle; here we only drain detent crossings.
static void scanReel() {
    int32_t delta = ReelMotor::takeDetentDelta();
    if (delta == 0) return;

    if (delta > 0) sendEvent(InputEvent::ENC_CW,  Button::PLAY, delta);
    else           sendEvent(InputEvent::ENC_CCW, Button::PLAY, -delta);
}

// ── Public API ─────────────────────────────────────────
void Input::init() {
    eventQueue = xQueueCreate(32, sizeof(InputMsg));

    if (!mcp.begin_I2C(0x20)) {
        Serial.println("[Input] MCP23017 not found!");
        return;
    }

    for (int i = 0; i < NUM_BTNS; i++) {
        mcp.pinMode(MCP_PINS[i], INPUT_PULLUP);
    }
    mcp.pinMode(MCP_LED_RED, OUTPUT);
    mcp.digitalWrite(MCP_LED_RED, LOW);

    mcp.setupInterrupts(true, false, LOW);
    for (int i = 0; i < NUM_BTNS; i++) {
        mcp.setupInterruptPin(MCP_PINS[i], CHANGE);
    }

    pinMode(PIN_MCP_INT, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_MCP_INT), mcpISR, FALLING);

    pinMode(PIN_ENC_SW, INPUT_PULLUP);

    Serial.println("[Input] Initialized — MCP23017 + BLDC reel");
}

void Input::taskFunc(void* param) {
    TickType_t lastWake = xTaskGetTickCount();

    while (true) {
        if (mcpIntFlag) {
            mcpIntFlag = false;
            mcp.getLastInterruptPin();  // clear MCP interrupt
        }
        scanButtons();
        scanReelButton();
        scanReel();

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(20));
    }
}

bool Input::poll(InputMsg& msg) {
    return xQueueReceive(eventQueue, &msg, 0) == pdTRUE;
}

QueueHandle_t Input::getQueue() {
    return eventQueue;
}