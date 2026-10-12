#include "sd_card.h"
#include "pins.h"
#include "errorlog.h"

static SPIClass spi_sd(HSPI);
static bool ready = false;

bool SDCard::init() {
    // Retry: a panic while SD was mid-I/O leaves the card in a bad state and
    // the very next begin() fails.  Re-init SPI + retry before giving up.
    for (int attempt = 1; attempt <= 3; attempt++) {
        spi_sd.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
        if (SD.begin(PIN_SD_CS, spi_sd, 25000000)) {
            ready = true;
            Serial.printf("[SD] Mounted. Size: %llu MB (attempt %d)\n",
                          SD.totalBytes() / (1024 * 1024), attempt);
            return true;
        }
        Log::warn("SD", "mount attempt %d failed", attempt);
        SD.end();
        delay(200);
    }
    Log::error("SD", "mount failed after 3 attempts — SD disabled");
    return false;
}

bool SDCard::isReady() { return ready; }
uint64_t SDCard::totalBytes() { return SD.totalBytes(); }
uint64_t SDCard::usedBytes() { return SD.usedBytes(); }