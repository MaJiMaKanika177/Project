#include "sd_card.h"
#include "pins.h"

static SPIClass spi_sd(HSPI);
static bool ready = false;

bool SDCard::init() {
    spi_sd.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
    if (!SD.begin(PIN_SD_CS, spi_sd, 25000000)) {
        Serial.println("[SD] Mount failed");
        return false;
    }
    ready = true;
    Serial.printf("[SD] Mounted. Size: %llu MB\n", SD.totalBytes() / (1024 * 1024));
    return true;
}

bool SDCard::isReady() { return ready; }
uint64_t SDCard::totalBytes() { return SD.totalBytes(); }
uint64_t SDCard::usedBytes() { return SD.usedBytes(); }