#include "usb_audio.h"
#include "pins.h"
#include <Arduino.h>

// ── USB UAC2 on ESP32-S3 ──────────────────────────────
// When ARDUINO_USB_CDC_ON_BOOT=1, the USB peripheral is used for Serial (CDC).
// UAC2 and CDC share the same USB-OTG hardware — they CANNOT coexist.
// To enable UAC2: set ARDUINO_USB_CDC_ON_BOOT=0 in platformio.ini,
// use UART0 (TX/RX pins) for Serial instead, and rebuild with TinyUSB UAC2
// descriptors.
//
// For now: stub that logs the conflict. When ready to go UAC2-only,
// flip the build flag and implement tud_audio callbacks here.

static bool active = false;

bool USBAudio::init() {
#if ARDUINO_USB_CDC_ON_BOOT
    Serial.println("[USB-UAC] Cannot init — USB CDC active (Serial over USB).");
    Serial.println("[USB-UAC] Set ARDUINO_USB_CDC_ON_BOOT=0 to enable UAC2.");
    Serial.println("[USB-UAC] Serial will move to UART0 (GPIO43 TX, GPIO44 RX).");
    return false;
#else
    // TODO: Full UAC2 implementation when CDC is disabled
    // 1. Configure TinyUSB audio descriptors (16-bit/44100Hz/stereo)
    // 2. Register tud_audio_rx_done_post_read_cb → i2s_write
    // 3. Handle volume/mute callbacks
    Serial.println("[USB-UAC] UAC2 mode — not yet implemented in this build");
    return false;
#endif
}

void USBAudio::stop() {
    active = false;
    Serial.println("[USB-UAC] Stopped");
}

bool USBAudio::isActive() { return active; }

void USBAudio::loop() {
    // No-op until UAC2 is implemented
}