#pragma once
#include <cstdint>

// The TP-7 reel is a BLDC gimbal motor driven with FOC plus an absolute
// magnetic angle sensor — it is BOTH the motor and the input encoder.
// Namespace name kept as ReelMotor so existing call sites don't change.
namespace ReelMotor {
    bool init();
    void focTask(void* param);   // FreeRTOS entry — tight FOC loop, own task

    // ── Playback-driven motion ──
    void play();       // slow steady spin
    void fastFwd();
    void rewind();
    void pause();      // hold position, no torque ramp
    void stop();
    void scrub(float velocity);  // rad/s, signed

    // ── Haptic knob mode ──
    // detentCount = detents per full revolution; 0 = free spin (no detents)
    void setDetents(int detentCount, float strength = 4.0f);
    void freeSpin();

    // ── Read ──
    float getAngle();        // continuous radians, unwrapped (can exceed 2π)
    float getAngleDeg();     // 0-360 wrapped — use for image rotation
    float getVelocity();     // rad/s
    int32_t takeDetentDelta();  // detent crossings since last call, then zeroes
    bool isReady();          // false if sensor/driver init failed

    enum class Mode : uint8_t { IDLE, SPIN, DETENT };
    Mode getMode();

    // Legacy alias kept so main.cpp's state machine reads unchanged
    enum class State : uint8_t { STOPPED, PLAY, FFW, REW, SCRUB };
    State getState();
}