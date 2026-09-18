#include "reel_motor.h"
#include "pins.h"
#include "config.h"
#include <Arduino.h>
#include <Wire.h>
#include <SimpleFOC.h>

// ── AS5600 absolute angle sensor (I2C, 12-bit, addr 0x36) ──────────────
#define AS5600_ADDR 0x36
#define AS5600_RAW_ANGLE_REG 0x0C

class AS5600Sensor : public Sensor {
public:
    explicit AS5600Sensor(TwoWire& w) : wire(w) {}

    bool probe() {
        wire.beginTransmission(AS5600_ADDR);
        return wire.endTransmission() == 0;
    }

    float getSensorAngle() override {
        wire.beginTransmission(AS5600_ADDR);
        wire.write(AS5600_RAW_ANGLE_REG);
        if (wire.endTransmission(false) != 0) return last;
        if (wire.requestFrom((uint8_t)AS5600_ADDR, (uint8_t)2) != 2) return last;
        uint8_t hi = wire.read();
        uint8_t lo = wire.read();
        uint16_t raw = ((uint16_t)hi << 8) | lo;   // 12-bit
        last = (float)raw * _2PI / 4096.0f;
        return last;
    }
private:
    TwoWire& wire;
    float last = 0;
};

// ── Objects ────────────────────────────────────────────────────────────
static BLDCMotor motor(REEL_POLE_PAIRS);
static BLDCDriver3PWM driver(PIN_BLDC_IN1, PIN_BLDC_IN2, PIN_BLDC_IN3, PIN_BLDC_EN);
static AS5600Sensor sensor(Wire1);

static bool ready = false;
static ReelMotor::Mode mode = ReelMotor::Mode::IDLE;
static ReelMotor::State legacyState = ReelMotor::State::STOPPED;

// FOC task <-> readers. 32-bit aligned scalars are atomic on Xtensa.
// ponytail: volatile scalars, add a mutex if a reader ever needs a consistent
// angle+velocity pair in the same instant.
static volatile float shAngle = 0;
static volatile float shVel   = 0;
static volatile float targetVel = 0;
static volatile int32_t detentDelta = 0;
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

static int   detents = 0;          // 0 = free spin
static float detentStrength = 4.0f;
static int32_t lastDetentIdx = 0;

bool ReelMotor::init() {
    Wire1.begin(PIN_ENC_SDA, PIN_ENC_SCL, 400000);

    if (!sensor.probe()) {
        Serial.println("[Reel] AS5600 not found on Wire1 — motor disabled");
        return false;
    }
    // sensor.init() is called by motor.init() → linkSensor
    motor.linkSensor(&sensor);

    driver.voltage_power_supply = REEL_VOLTAGE_PSU;
    driver.voltage_limit = REEL_VOLTAGE_LIMIT;
    if (!driver.init()) {
        Serial.println("[Reel] DRV8313 driver init failed");
        return false;
    }
    motor.linkDriver(&driver);

    motor.voltage_limit = REEL_VOLTAGE_LIMIT;
    motor.velocity_limit = 40.0f;
    motor.controller = MotionControlType::torque;
    motor.torque_controller = TorqueControlType::voltage;
    motor.foc_modulation = FOCModulationType::SpaceVectorPWM;

    motor.PID_velocity.P = 0.25f;
    motor.PID_velocity.I = 2.0f;
    motor.PID_velocity.D = 0.0f;
    motor.LPF_velocity.Tf = 0.02f;

    motor.init();
    if (motor.initFOC() != 1) {          // aligns sensor, spins briefly
        Serial.println("[Reel] initFOC failed (check phase wiring / pole pairs)");
        return false;
    }

    lastDetentIdx = 0;
    ready = true;
    Serial.println("[Reel] BLDC + AS5600 ready");
    return true;
}

void ReelMotor::focTask(void* param) {
    while (true) {
        if (!ready) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }

        // ~4 FOC iterations per tick → ~4kHz commutation, still yields to audio
        for (int i = 0; i < 4; i++) motor.loopFOC();

        float a = motor.shaft_angle;
        shAngle = a;
        shVel   = motor.shaft_velocity;

        if (mode == Mode::DETENT && detents > 0) {
            float width = _2PI / (float)detents;
            int32_t idx = (int32_t)lroundf(a / width);
            if (idx != lastDetentIdx) {
                portENTER_CRITICAL(&mux);
                detentDelta += (idx - lastDetentIdx);
                portEXIT_CRITICAL(&mux);
                lastDetentIdx = idx;
            }
            // spring toward nearest detent centre
            motor.move((idx * width - a) * detentStrength);
        } else if (mode == Mode::SPIN) {
            motor.move(targetVel);
        } else {
            motor.move(0);
        }

        vTaskDelay(1);
    }
}

// ── Motion ─────────────────────────────────────────────────────────────
static void spin(float vel, ReelMotor::State s) {
    motor.controller = MotionControlType::velocity;
    targetVel = vel;
    mode = ReelMotor::Mode::SPIN;
    legacyState = s;
}

void ReelMotor::play()    { spin(REEL_PLAY_VEL,  State::PLAY); }
void ReelMotor::fastFwd() { spin(REEL_FFW_VEL,   State::FFW);  }
void ReelMotor::rewind()  { spin(-REEL_FFW_VEL,  State::REW);  }

void ReelMotor::scrub(float velocity) {
    spin(constrain(velocity, -40.0f, 40.0f), State::SCRUB);
}

void ReelMotor::pause() {
    // Hand back to the detent/idle controller — reel holds where it stopped.
    targetVel = 0;
    mode = (detents > 0) ? Mode::DETENT : Mode::IDLE;
    motor.controller = MotionControlType::torque;
    lastDetentIdx = (detents > 0)
        ? (int32_t)lroundf(shAngle / (_2PI / (float)detents)) : 0;
    legacyState = State::STOPPED;
}

void ReelMotor::stop() { pause(); }

// ── Haptic knob ────────────────────────────────────────────────────────
void ReelMotor::setDetents(int detentCount, float strength) {
    detents = detentCount;
    detentStrength = strength;
    if (detentCount > 0) {
        motor.controller = MotionControlType::torque;
        lastDetentIdx = (int32_t)lroundf(shAngle / (_2PI / (float)detentCount));
        mode = Mode::DETENT;
    } else {
        mode = Mode::IDLE;
    }
    legacyState = State::STOPPED;
}

void ReelMotor::freeSpin() {
    detents = 0;
    motor.controller = MotionControlType::torque;
    mode = Mode::IDLE;
    legacyState = State::STOPPED;
}

// ── Read ───────────────────────────────────────────────────────────────
float ReelMotor::getAngle() { return shAngle; }

float ReelMotor::getAngleDeg() {
    float d = fmodf(shAngle * RAD_TO_DEG, 360.0f);
    return d < 0 ? d + 360.0f : d;
}

float ReelMotor::getVelocity() { return shVel; }

int32_t ReelMotor::takeDetentDelta() {
    // Atomic swap to avoid race with FOC task on core 1
    portENTER_CRITICAL(&mux);
    int32_t d = detentDelta;
    detentDelta = 0;
    portEXIT_CRITICAL(&mux);
    return d;
}

bool ReelMotor::isReady() { return ready; }
ReelMotor::Mode ReelMotor::getMode() { return mode; }
ReelMotor::State ReelMotor::getState() { return legacyState; }