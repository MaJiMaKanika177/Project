#include "errorlog.h"
#include <LittleFS.h>
#include <stdarg.h>
#include <string.h>

// ── RAM ring (fast path, always available after init) ─────────────
// 24 entries x 128B = 3KB SRAM. Enough to cover a whole boot sequence.
static constexpr int RING_N = 24;
static constexpr int RING_LEN = 128;
static char ring[RING_N][RING_LEN];
static uint8_t ringNext = 0;
static uint8_t ringCount = 0;
static bool fsReady = false;
static int bootErrors = 0;

static void push(const char* level, const char* src, const char* msg) {
    char* slot = ring[ringNext];
    int n = snprintf(slot, RING_LEN, "%s %s: %s", level, src, msg);
    if (n < 0) return;
    ringNext = (ringNext + 1) % RING_N;
    if (ringCount < RING_N) ringCount++;
    if (level[0] == 'E') bootErrors++;
    Serial.printf("[%s] %s: %s\n", src, level, msg);
    if (fsReady) {
        // Persist. /errors.log is capped by replaying only the tail at boot,
        // so it never grows unbounded enough to matter.
        File f = LittleFS.open("/errors.log", "a");
        if (f) {
            f.printf("%lu %s %s: %s\n", millis(), level, src, msg);
            f.close();
        } else {
            fsReady = false;   // card/flash gone; ring+serial still work
        }
    }
}

void Log::init() {
    bootErrors = 0;
}

void Log::attachFs() {
    fsReady = LittleFS.begin(false);
    if (!fsReady) return;
    // Replay last boot's tail so a crash-loop is diagnosable with no serial
    // attached: read last ~2KB of /errors.log.
    File f = LittleFS.open("/errors.log", "r");
    if (!f) return;
    if (f.size() > 2048) f.seek(f.size() - 2048);
    Serial.println("──── previous boot errors (tail of /errors.log) ────");
    while (f.available()) {
        String line = f.readStringUntil('\n');
        if (line.length()) {
            // push() re-adds the [src] prefix; strip the stored "src: " is
            // overkill — just print raw so the log line is verifiable.
            Serial.println(line);
        }
    }
    f.close();
    // Rotate: truncate so the file stays small
    f = LittleFS.open("/errors.log", "w");
    if (f) f.close();
}

void Log::error(const char* src, const char* fmt, ...) {
    char msg[RING_LEN];
    va_list ap; va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    push("E", src, msg);
}

void Log::warn(const char* src, const char* fmt, ...) {
    char msg[RING_LEN];
    va_list ap; va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    push("W", src, msg);
}

static String jsonEscape(const char* s) {
    String out;
    for (const char* p = s; *p; p++) {
        switch (*p) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            default:
                if ((unsigned char)*p < 0x20) {
                    char u[7]; snprintf(u, sizeof(u), "\\u%04x", *p); out += u;
                } else out += *p;
        }
    }
    return out;
}

String Log::ringJSON() {
    String s;
    s.reserve(1024 + ringCount * (RING_LEN + 4));
    s += "{\"count\":"; s += String(ringCount); s += ",\"entries\":[";
    for (int i = 0; i < ringCount; i++) {
        if (i) s += ",";
        s += "\""; s += jsonEscape(ring[i]); s += "\"";
    }
    s += "]}";
    return s;
}

int Log::bootErrorCount() { return bootErrors; }
