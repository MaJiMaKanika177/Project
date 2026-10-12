#include "playlist.h"
#include <Arduino.h>
#include <esp_task_wdt.h>
#include <SD.h>
#include <LittleFS.h>
#include <cstring>
#include "util.h"
#include "errorlog.h"

// ── Init ──────────────────────────────────────────────
bool Playlist::init() {
    // Allocate large arrays in PSRAM (saves ~146KB SRAM)
    tracks = (char(*)[256])ps_calloc(MAX_TRACKS, 256);
    shuffleOrder = (int*)ps_calloc(MAX_TRACKS, sizeof(int));
    favs = (char(*)[256])ps_calloc(MAX_FAVS, 256);
    if (!tracks || !shuffleOrder || !favs) {
        Serial.println("[Playlist] PSRAM alloc failed — falling back to heap");
        if (!tracks) tracks = (char(*)[256])calloc(MAX_TRACKS, 256);
        if (!shuffleOrder) shuffleOrder = (int*)calloc(MAX_TRACKS, sizeof(int));
        if (!favs) favs = (char(*)[256])calloc(MAX_FAVS, 256);
    }
    if (!tracks || !shuffleOrder || !favs) {
        Log::error("Playlist", "alloc failed (PSRAM+heap) — playlist disabled");
        return false;
    }

    // LittleFS.begin(true) formats on first mount — that blocks 10-30s on
    // 16MB flash and triggers Task WDT.  Try mount-only first; if it fails,
    // format with WDT fed, then re-mount.
    if (!LittleFS.begin(false)) {
        Serial.println("[Playlist] LittleFS not formatted — formatting...");
        esp_task_wdt_reset();
        LittleFS.format();
        esp_task_wdt_reset();
        if (!LittleFS.begin(false)) {
            Log::error("Playlist", "LittleFS mount failed after format");
            // Non-fatal: favorites/resume won't persist, but player works
        }
    }
    loadFavorites();
    Serial.printf("[Playlist] Init OK — %d favorites\n", favCount);
    return true;
}

bool Playlist::isAudioFile(const char* name) {
    return ::isAudioFile(name);  // delegate to shared util.h
}

// ── SD scan ───────────────────────────────────────────
// Junk/system dirs never contain music — and Android-formatted cards carry
// broken LFNs (esp. /Android) that crash FatFs's openNextFile on this chip.
// Skipping them dodges the crash AND keeps the playlist clean.
static bool isJunkDir(const char* dir) {
    const char* base = strrchr(dir, '/');
    base = base ? base + 1 : dir;
    if (!base[0]) return false;   // root itself — never skip

    static const char* const skip[] = {
        "android", "system volume information", "$recycle.bin",
        "lost.dir", ".trash", ".spotlight-v100", ".fseventsd",
        ".temporaryitems", "lazy", "recovery", "backups"
    };
    for (auto s : skip) {
        if (strncasecmp(base, s, strlen(s)) == 0) {
            // prefix match (".trash" matches ".trashBin_File")
            Serial.printf("[Scan] skip junk dir: %s\n", dir);
            return true;
        }
    }
    return false;
}

void Playlist::scanDirRecursive(const char* dir, int depth) {
    if (depth > 4 || trackCount >= MAX_TRACKS) return;
    if (isJunkDir(dir)) return;

    Serial.printf("[Scan] open dir='%s' depth=%d\n", dir, depth);
    File root = SD.open(dir);
    if (!root) { Serial.println("[Scan] ROOT OPEN FAILED"); return; }
    if (!root.isDirectory()) { Serial.println("[Scan] ROOT NOT A DIR"); root.close(); return; }

    // Collect subdir names first, recurse AFTER root is closed.
    // FatFs on ESP32 can't hold two directory streams open across an
    // SD.open() — concurrent handles made every subdir open fail.
    char subDirs[8][256];
    int nSub = 0;

    File f = root.openNextFile();
    while (f && trackCount < MAX_TRACKS) {
        const char* name = f.name();
        char full[256];

        if (name[0] == '/') {
            strncpy(full, name, sizeof(full) - 1);
            full[sizeof(full) - 1] = '\0';
        } else {
            snprintf(full, sizeof(full), "%s%s%s",
                     dir, (dir[strlen(dir) - 1] == '/' ? "" : "/"), name);
        }

        if (f.isDirectory()) {
            if (nSub < 8) {
                strncpy(subDirs[nSub], full, 255);
                subDirs[nSub][255] = '\0';
                nSub++;
            }
            Serial.printf("[Scan] queued subdir[%d]='%s'\n", nSub - 1, subDirs[nSub - 1]);
        } else {
            Serial.printf("[Scan] %s → audio=%d\n", full, isAudioFile(full) ? 1 : 0);
            if (isAudioFile(full)) {
                strncpy(tracks[trackCount], full, 255);
                tracks[trackCount][255] = '\0';
                trackCount++;
            }
        }
        f.close();
        f = root.openNextFile();
    }
    root.close();

    // Recurse now that no other directory handle is open.
    for (int i = 0; i < nSub && trackCount < MAX_TRACKS; i++) {
        scanDirRecursive(subDirs[i], depth + 1);
    }
}

int Playlist::scanSD(const char* rootDir) {
    trackCount = 0;
    currentIndex = -1;
    scanDirRecursive(rootDir, 0);
    if (mode == PlayMode::SHUFFLE) generateShuffle();
    Serial.printf("[Playlist] Scanned %d tracks\n", trackCount);
    return trackCount;
}

// ── M3U ───────────────────────────────────────────────
bool Playlist::loadM3U(const char* path) {
    File f = SD.open(path);
    if (!f) {
        Serial.printf("[Playlist] Can't open M3U: %s\n", path);
        return false;
    }

    trackCount = 0;
    currentIndex = -1;

    while (f.available() && trackCount < MAX_TRACKS) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0 || line.startsWith("#")) continue;  // skip comments/EXTINF

        if (line[0] != '/') {
            // Relative to M3U location
            char base[256];
            strncpy(base, path, sizeof(base) - 1);
            base[sizeof(base) - 1] = '\0';
            char* slash = strrchr(base, '/');
            if (slash) *(slash + 1) = '\0';
            snprintf(tracks[trackCount], 256, "%s%s", base, line.c_str());
        } else {
            strncpy(tracks[trackCount], line.c_str(), 255);
            tracks[trackCount][255] = '\0';
        }
        trackCount++;
    }
    f.close();

    if (mode == PlayMode::SHUFFLE) generateShuffle();
    Serial.printf("[Playlist] Loaded M3U: %d tracks\n", trackCount);
    return trackCount > 0;
}

bool Playlist::saveM3U(const char* path) {
    File f = SD.open(path, FILE_WRITE);
    if (!f) return false;
    f.println("#EXTM3U");
    for (int i = 0; i < trackCount; i++) f.println(tracks[i]);
    f.close();
    Serial.printf("[Playlist] Saved M3U: %s (%d tracks)\n", path, trackCount);
    return true;
}

// ── Shuffle ───────────────────────────────────────────
void Playlist::generateShuffle() {
    for (int i = 0; i < trackCount; i++) shuffleOrder[i] = i;
    // Fisher-Yates
    for (int i = trackCount - 1; i > 0; i--) {
        int j = random(i + 1);
        int t = shuffleOrder[i];
        shuffleOrder[i] = shuffleOrder[j];
        shuffleOrder[j] = t;
    }
    shufflePos = 0;
}

// ── Navigation ────────────────────────────────────────
const char* Playlist::current() {
    if (currentIndex < 0 || currentIndex >= trackCount) return "";
    return tracks[currentIndex];
}

const char* Playlist::next() {
    if (trackCount == 0) return "";

    switch (mode) {
        case PlayMode::REPEAT_ONE:
            if (currentIndex < 0) currentIndex = 0;
            break;

        case PlayMode::SHUFFLE:
            shufflePos++;
            if (shufflePos >= trackCount) {
                generateShuffle();  // reshuffle on wrap
            }
            currentIndex = shuffleOrder[shufflePos];
            break;

        case PlayMode::SEQUENTIAL:
        case PlayMode::REPEAT_ALL:
        default:
            currentIndex++;
            if (currentIndex >= trackCount) currentIndex = 0;
            break;
    }
    return tracks[currentIndex];
}

const char* Playlist::prev() {
    if (trackCount == 0) return "";

    switch (mode) {
        case PlayMode::REPEAT_ONE:
            if (currentIndex < 0) currentIndex = 0;
            break;

        case PlayMode::SHUFFLE:
            shufflePos--;
            if (shufflePos < 0) shufflePos = trackCount - 1;
            currentIndex = shuffleOrder[shufflePos];
            break;

        default:
            currentIndex--;
            if (currentIndex < 0) currentIndex = trackCount - 1;
            break;
    }
    return tracks[currentIndex];
}

const char* Playlist::getTrack(int index) {
    if (index < 0 || index >= trackCount) return "";
    return tracks[index];
}

int Playlist::getIndex() { return currentIndex; }
int Playlist::getCount() { return trackCount; }

void Playlist::setIndex(int idx) {
    if (idx < 0 || idx >= trackCount) return;
    currentIndex = idx;
    if (mode == PlayMode::SHUFFLE) {
        // Sync shuffle position to this track
        for (int i = 0; i < trackCount; i++) {
            if (shuffleOrder[i] == idx) { shufflePos = i; break; }
        }
    }
}

void Playlist::setMode(PlayMode m) {
    mode = m;
    if (m == PlayMode::SHUFFLE) generateShuffle();
}

PlayMode Playlist::getMode() { return mode; }

// ── Favorites (LittleFS /favorites.txt) ───────────────
void Playlist::loadFavorites() {
    favCount = 0;
    File f = LittleFS.open("/favorites.txt", "r");
    if (!f) return;
    while (f.available() && favCount < MAX_FAVS) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;
        strncpy(favs[favCount], line.c_str(), 255);
        favs[favCount][255] = '\0';
        favCount++;
    }
    f.close();
}

void Playlist::saveFavorites() {
    File f = LittleFS.open("/favorites.txt", "w");
    if (!f) return;
    for (int i = 0; i < favCount; i++) f.println(favs[i]);
    f.close();
}

bool Playlist::isFavorite(const char* path) {
    for (int i = 0; i < favCount; i++) {
        if (strcmp(favs[i], path) == 0) return true;
    }
    return false;
}

bool Playlist::toggleFavorite(const char* path) {
    for (int i = 0; i < favCount; i++) {
        if (strcmp(favs[i], path) == 0) {
            // Remove — shift down
            for (int j = i; j < favCount - 1; j++) {
                strcpy(favs[j], favs[j + 1]);
            }
            favCount--;
            saveFavorites();
            return false;  // now not favorite
        }
    }
    if (favCount < MAX_FAVS) {
        strncpy(favs[favCount], path, 255);
        favs[favCount][255] = '\0';
        favCount++;
        saveFavorites();
    }
    return true;  // now favorite
}

int Playlist::getFavoriteCount() { return favCount; }

// ── Resume points (LittleFS /resume.txt) ──────────────
// Format: one entry per line — "position\tpath"
// Newest first; LRU evict at MAX_RESUME. Plain text, not JSON — no
// ArduinoJson dep needed for a two-field record.
void Playlist::saveResumePoint(const char* path, uint32_t position) {
    // Read existing entries (skip the one we're replacing)
    static char lines[MAX_RESUME][300];
    int n = 0;

    File rf = LittleFS.open("/resume.txt", "r");
    if (rf) {
        while (rf.available() && n < MAX_RESUME - 1) {
            String line = rf.readStringUntil('\n');
            line.trim();
            if (line.length() == 0) continue;
            int tab = line.indexOf('\t');
            if (tab < 0) continue;
            if (line.substring(tab + 1) == path) continue;  // drop old entry
            strncpy(lines[n], line.c_str(), 299);
            lines[n][299] = '\0';
            n++;
        }
        rf.close();
    }

    File wf = LittleFS.open("/resume.txt", "w");
    if (!wf) return;
    wf.printf("%u\t%s\n", position, path);   // newest first
    for (int i = 0; i < n; i++) wf.println(lines[i]);
    wf.close();
}

bool Playlist::getResumePoint(const char* path, uint32_t& position) {
    File f = LittleFS.open("/resume.txt", "r");
    if (!f) return false;

    bool found = false;
    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        int tab = line.indexOf('\t');
        if (tab < 0) continue;
        if (line.substring(tab + 1) == path) {
            position = (uint32_t)line.substring(0, tab).toInt();
            found = true;
            break;
        }
    }
    f.close();
    return found;
}