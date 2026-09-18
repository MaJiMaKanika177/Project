#include "battery.h"
#include "pins.h"
#include <Arduino.h>

static float avgVoltage = 0;
static bool firstRead = true;

static float readRaw() {
    uint32_t raw = analogReadMilliVolts(PIN_BAT_ADC);
    float pinV = raw / 1000.0f;
    return pinV * ((BAT_R1 + BAT_R2) / BAT_R2);
}

bool Battery::init() {
    pinMode(PIN_BAT_ADC, INPUT);
    analogSetPinAttenuation(PIN_BAT_ADC, ADC_11db);
    avgVoltage = readRaw();
    firstRead = false;
    Serial.printf("[Battery] ADC on GPIO18, initial %.2fV\n", avgVoltage);
    return true;
}

float Battery::voltage() {
    return avgVoltage;
}

float Battery::percent() {
    float v = avgVoltage;
    if (v >= 4.2f) return 100.0f;
    if (v <= 3.2f) return 0.0f;
    return (v - 3.2f) / (4.2f - 3.2f) * 100.0f;
}

bool Battery::isLow() { return percent() < 10.0f; }

void Battery::poll() {
    // Exponential moving average, alpha=0.1 → smooth over ~10 calls
    float v = readRaw();
    avgVoltage = avgVoltage * 0.9f + v * 0.1f;
}