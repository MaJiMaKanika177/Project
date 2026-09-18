#pragma once
#include <SD.h>
#include <SPI.h>

namespace SDCard {
    bool init();
    bool isReady();
    uint64_t totalBytes();
    uint64_t usedBytes();
}