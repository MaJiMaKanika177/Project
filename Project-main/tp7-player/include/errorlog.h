#pragma once
#include <Arduino.h>

// Central error logger. Every subsystem failure goes through Log::error():
//  1. printed to Serial immediately,
//  2. stored in a RAM ring — readable via GET /api/errors (no serial needed),
//  3. appended to /errors.log on LittleFS — survives crash/reboot; the tail
//     of the previous boot is replayed to serial at next boot (Log::attachFs).
//
// Call order: Log::init() right after Serial is up; Log::attachFs() once
// LittleFS is mounted (playlist.init()). Errors before attachFs are ring+serial only.
namespace Log {
    void init();
    void attachFs();
    void error(const char* src, const char* fmt, ...);
    void warn(const char* src, const char* fmt, ...);
    String ringJSON();          // {"count":N,"entries":["E SD: ...",...]}
    int  bootErrorCount();      // E-level entries since boot (for UI badge)
}
