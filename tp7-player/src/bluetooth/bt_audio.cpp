#include "bt_audio.h"
#include <Arduino.h>

// ── ESP32-S3 does NOT support Bluetooth Classic (A2DP) ──
// Only the original ESP32 (dual-mode BT) can do A2DP sink/source.
// ESP32-S3 has BLE only. To add BT audio, either:
//   1. Use original ESP32 instead of S3
//   2. Add external BT module (e.g. BM83) over UART/I2S
//   3. Use BLE audio (LE Audio / LC3) — not yet mature on ESP32-S3
//
// This stub logs the limitation and returns gracefully.

static BTAudio::Mode currentMode = BTAudio::Mode::OFF;
static char trackTitle[1] = {};

void BTAudio::initSink(const char* deviceName) {
    Serial.printf("[BT] A2DP SINK not available on ESP32-S3 (no Classic BT)\n");
    Serial.println("[BT] Use original ESP32 or external BT module (BM83)");
    (void)deviceName;
}

void BTAudio::initSource(const char* targetName) {
    Serial.println("[BT] A2DP SOURCE not available on ESP32-S3");
    (void)targetName;
}

void BTAudio::disconnect() {}
void BTAudio::stop() { currentMode = Mode::OFF; }
bool BTAudio::isConnected() { return false; }
BTAudio::Mode BTAudio::getMode() { return currentMode; }
const char* BTAudio::getTrackTitle() { return trackTitle; }
void BTAudio::setVolumeCallback(VolumeCallback cb) { (void)cb; }