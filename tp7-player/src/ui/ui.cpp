#include "ui.h"
#include "../drivers/display.h"
#include "../drivers/reel_motor.h"
#include "config.h"
#include "pins.h"
#include "util.h"
#include <Arduino.h>
#include <SD.h>
#include <cstring>
#include <cstdio>
#include <cmath>

namespace D = Display;

// ── State ──────────────────────────────────────────────
static UIState st;
static AudioPlayer* gPlayer = nullptr;
static Equalizer* gEq = nullptr;

// File browser cache — allocated in PSRAM via init()
static constexpr int MAX_FILES = 128;
static char (*fileList)[64] = nullptr;   // short names
static char (*filePaths)[256] = nullptr;
static int fileCount = 0;
static char currentDir[256] = "/";

// Dirty flag — skip SPI push if nothing changed
static bool dirty = true;
static void markDirty() { dirty = true; }

// Colours (RGB565)
static constexpr uint16_t C_BG     = 0x0000;
static constexpr uint16_t C_FG     = 0xFFFF;
static constexpr uint16_t C_DIM    = 0x8410;
static constexpr uint16_t C_RING   = 0x2945;
static constexpr uint16_t C_ACCENT = 0xF9A0;   // TP-7 orange
static constexpr uint16_t C_RED    = 0xF800;

static constexpr int REEL_SPOKES = 6;

// ── Reel mode per screen ───────────────────────────────
// Playback spins the reel (decorative, angle drives the artwork).
// Menus turn it into a haptic detent knob.
static void applyReelMode() {
    if (!ReelMotor::isReady()) return;
    switch (st.screen) {
        case Screen::FILE_BROWSER:
            ReelMotor::setDetents(fileCount > 1 ? min(fileCount, 48) : 24, 5.0f);
            break;
        case Screen::EQ:       ReelMotor::setDetents(24, 6.0f); break;
        case Screen::SETTINGS: ReelMotor::setDetents(20, 5.0f); break;
        case Screen::NOW_PLAYING:
            if (gPlayer && gPlayer->getState() == PlayState::PLAYING) ReelMotor::play();
            else ReelMotor::setDetents(30, 4.0f);   // volume knob when idle
            break;
        default: ReelMotor::freeSpin(); break;
    }
}

static void scanDir(const char* dir) {
    fileCount = 0;
    File root = SD.open(dir);
    if (!root || !root.isDirectory()) return;

    File f = root.openNextFile();
    while (f && fileCount < MAX_FILES) {
        const char* name = f.name();
        if (f.isDirectory()) {
            snprintf(fileList[fileCount], 64, "[%s]", name);
            snprintf(filePaths[fileCount], 256, "%s%s/", dir, name);
            fileCount++;
        } else if (isAudioFile(name)) {
            strncpy(fileList[fileCount], name, 63);
            snprintf(filePaths[fileCount], 256, "%s%s", dir, name);
            fileCount++;
        }
        f = root.openNextFile();
    }
    root.close();
    st.fileCount = fileCount;
    applyReelMode();
}

static const char* getFilename(const char* path) {
    const char* slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

// ── Draw helpers ───────────────────────────────────────
static void drawStatusArc(LGFX_Sprite& g) {
    g.setFont(&fonts::Font2);
    g.setTextDatum(top_center);
    g.setTextColor(C_DIM, C_BG);

    char line[24] = {};
    int n = 0;
    if (st.wifiConnected) n += snprintf(line + n, sizeof(line) - n, "wifi ");
    if (st.btConnected)   n += snprintf(line + n, sizeof(line) - n, "bt ");
    if (st.batteryPct >= 0) snprintf(line + n, sizeof(line) - n, "%d%%", (int)st.batteryPct);
    if (line[0]) g.drawString(line, D::CX, 16);

    g.drawCircle(D::CX, D::CY, D::R - 1, C_RING);
}

// The rotating artwork: spokes follow the true shaft angle from the sensor.
static void drawReel(LGFX_Sprite& g, int radius, float angleDeg, uint16_t col) {
    const float base = angleDeg * DEG_TO_RAD;
    const float step = TWO_PI / REEL_SPOKES;
    const int hub = radius / 4;

    for (int i = 0; i < REEL_SPOKES; i++) {
        float a = base + i * step;
        float c = cosf(a), s = sinf(a);
        g.drawLine(D::CX + (int)(c * hub),      D::CY + (int)(s * hub),
                   D::CX + (int)(c * radius),   D::CY + (int)(s * radius), col);
        // spoke tip dot — sells the rotation at a glance
        g.fillCircle(D::CX + (int)(c * radius), D::CY + (int)(s * radius), 3, col);
    }
    g.drawCircle(D::CX, D::CY, radius, C_RING);
    g.fillCircle(D::CX, D::CY, hub, C_BG);
    g.drawCircle(D::CX, D::CY, hub, col);
}

// ── Screen renderers ───────────────────────────────────
static void renderSplash(LGFX_Sprite& g) {
    g.setTextDatum(middle_center);
    g.setTextColor(C_FG, C_BG);
    g.setFont(&fonts::FreeSansBold18pt7b);
    g.drawString("TP-7", D::CX, D::CY - 12);
    g.setFont(&fonts::Font2);
    g.setTextColor(C_DIM, C_BG);
    g.drawString(FW_VERSION, D::CX, D::CY + 24);
    g.drawCircle(D::CX, D::CY, D::R - 2, C_RING);
}

static void renderNowPlaying(LGFX_Sprite& g) {
    drawStatusArc(g);

    PlayState ps = gPlayer->getState();
    const char* track = gPlayer->getCurrentTrack();
    float prog = gPlayer->getProgress();

    // Reel artwork rotating with the shaft
    drawReel(g, 96, ReelMotor::getAngleDeg(),
             ps == PlayState::PLAYING ? C_ACCENT : C_DIM);

    // Progress ring on the outer edge
    if (prog > 0.001f) {
        g.fillArc(D::CX, D::CY, D::R - 8, D::R - 4, 0, (int)(360 * prog), C_ACCENT);
    }

    // Track name — two lines max, scrolled if longer
    g.setFont(&fonts::Font2);
    g.setTextDatum(middle_center);
    g.setTextColor(C_FG, C_BG);

    const char* name = getFilename(track);
    int nameLen = strlen(name);
    const int maxChars = 20;

    char buf[32] = {};
    if (nameLen > maxChars) {
        uint32_t now = millis();
        if (now - st.lastScrollTime > 220) {
            st.scrollOffset++;
            if (st.scrollOffset > nameLen - maxChars + 3) st.scrollOffset = 0;
            st.lastScrollTime = now;
        }
        int start = st.scrollOffset < nameLen ? st.scrollOffset : 0;
        strncpy(buf, name + start, maxChars);
    } else {
        strncpy(buf, name[0] ? name : "No track", maxChars);
    }
    g.fillRect(D::CX - 80, D::CY - 34, 160, 18, C_BG);
    g.drawString(buf, D::CX, D::CY - 25);

    // Play state glyph in the hub
    g.setTextColor(ps == PlayState::PLAYING ? C_ACCENT : C_DIM, C_BG);
    g.setFont(&fonts::Font4);
    const char* icon = ps == PlayState::PLAYING ? ">" : ps == PlayState::PAUSED ? "||" : "#";
    g.drawString(icon, D::CX, D::CY);

    // Volume + format below the hub
    g.setFont(&fonts::Font2);
    g.setTextColor(C_DIM, C_BG);
    char sub[24];
    const char* ext = strrchr(track, '.');
    snprintf(sub, sizeof(sub), "%d%%  %s%s",
             (int)(gPlayer->getVolume() * 100),
             ext ? ext + 1 : "",
             gEq->isFlat() ? "" : "  EQ");
    g.fillRect(D::CX - 70, D::CY + 26, 140, 16, C_BG);
    g.drawString(sub, D::CX, D::CY + 34);
}

static void renderFileBrowser(LGFX_Sprite& g) {
    drawStatusArc(g);

    g.setTextDatum(middle_center);
    g.setFont(&fonts::Font2);

    // Faint reel behind the list — still tracks the knob
    drawReel(g, 108, ReelMotor::getAngleDeg(), C_RING);

    g.setTextColor(C_DIM, C_BG);
    g.drawString(currentDir, D::CX, 42);

    // 5 rows; round screen means shorter rows at top/bottom
    const int visible = 5;
    const int rowH = 24;
    int start = st.menuScroll;
    for (int i = 0; i < visible && (start + i) < fileCount; i++) {
        int idx = start + i;
        int y = D::CY - (visible / 2) * rowH + i * rowH;
        // chord half-width at this y, minus margin
        int dy = abs(y - D::CY);
        int halfW = (int)sqrtf((float)(D::R * D::R - dy * dy)) - 12;
        if (halfW < 20) continue;

        if (idx == st.menuIndex) {
            g.fillRoundRect(D::CX - halfW, y - rowH / 2 + 2, halfW * 2, rowH - 4, 6, C_ACCENT);
            g.setTextColor(C_BG, C_ACCENT);
        } else {
            g.setTextColor(C_FG, C_BG);
        }
        // clip name to the chord
        int maxCh = halfW * 2 / 7;
        char row[40];
        strncpy(row, fileList[idx], sizeof(row) - 1);
        row[sizeof(row) - 1] = '\0';
        if ((int)strlen(row) > maxCh && maxCh > 3) row[maxCh] = '\0';
        g.drawString(row, D::CX, y);
    }

    if (fileCount == 0) {
        g.setTextColor(C_DIM, C_BG);
        g.drawString("no audio files", D::CX, D::CY);
    }
}

static void renderEQ(LGFX_Sprite& g) {
    drawStatusArc(g);

    // 10 bands as radial bars — natural fit for a round panel
    const int r0 = 46, rMax = 104;
    for (int i = 0; i < EQ_BANDS; i++) {
        float a = (-90.0f + (i - (EQ_BANDS - 1) / 2.0f) * 16.0f) * DEG_TO_RAD;
        float gain = gEq->getGain(i);
        int len = (int)((gain + 12.0f) / 24.0f * (rMax - r0));
        uint16_t col = (i == st.eqBand) ? C_ACCENT : C_DIM;

        float c = cosf(a), s = sinf(a);
        g.drawWideLine(D::CX + c * r0, D::CY + s * r0,
                       D::CX + c * (r0 + len), D::CY + s * (r0 + len), 7, col);
        if (i == st.eqBand) {
            g.drawCircle(D::CX + c * (r0 + len), D::CY + s * (r0 + len), 6, C_FG);
        }
    }
    // zero-gain reference arc
    int rMid = r0 + (rMax - r0) / 2;
    g.drawCircle(D::CX, D::CY, rMid, C_RING);

    g.setTextDatum(middle_center);
    g.setFont(&fonts::Font2);
    g.setTextColor(C_FG, C_BG);
    g.drawString("EQ", D::CX, D::CY + 40);

    char dbStr[16];
    snprintf(dbStr, sizeof(dbStr), "%s %+.0f dB",
             Equalizer::BAND_LABELS[st.eqBand], gEq->getGain(st.eqBand));
    g.setTextColor(C_ACCENT, C_BG);
    g.drawString(dbStr, D::CX, D::CY + 62);
}

static void renderSettings(LGFX_Sprite& g) {
    drawStatusArc(g);

    static const char* menuItems[] = { "WiFi", "Bluetooth", "EQ Preset", "About", "Back" };
    static const int itemCount = 5;

    g.setTextDatum(middle_center);
    g.setFont(&fonts::Font2);
    g.setTextColor(C_DIM, C_BG);
    g.drawString("SETTINGS", D::CX, 46);

    const int rowH = 26;
    for (int i = 0; i < itemCount; i++) {
        int y = D::CY - (itemCount / 2) * rowH + i * rowH + 8;
        int dy = abs(y - D::CY);
        int halfW = (int)sqrtf((float)(D::R * D::R - dy * dy)) - 14;
        if (halfW < 20) continue;

        if (i == st.menuIndex) {
            g.fillRoundRect(D::CX - halfW, y - rowH / 2 + 2, halfW * 2, rowH - 4, 6, C_ACCENT);
            g.setTextColor(C_BG, C_ACCENT);
        } else {
            g.setTextColor(C_FG, C_BG);
        }
        g.drawString(menuItems[i], D::CX, y);
    }
}

// ── Input handling per screen ──────────────────────────
static void inputNowPlaying(const InputMsg& msg) {
    switch (msg.event) {
        case InputEvent::BTN_PRESS:
            if (msg.button == Button::PLAY) {
                if (gPlayer->getState() == PlayState::PLAYING) gPlayer->pause();
                else if (gPlayer->getState() == PlayState::PAUSED) gPlayer->resume();
                applyReelMode();
            }
            else if (msg.button == Button::ROCKER_UP) {
                gPlayer->setVolume(gPlayer->getVolume() + 0.05f);
            }
            else if (msg.button == Button::ROCKER_DN) {
                gPlayer->setVolume(gPlayer->getVolume() - 0.05f);
            }
            else if (msg.button == Button::MENU) {
                st.screen = Screen::FILE_BROWSER;
                st.menuIndex = 0;
                st.menuScroll = 0;
                scanDir(currentDir);
            }
            break;
        case InputEvent::BTN_LONG_PRESS:
            if (msg.button == Button::PLAY) { gPlayer->stop(); applyReelMode(); }
            if (msg.button == Button::MENU) {
                st.screen = Screen::SETTINGS;
                st.menuIndex = 0;
                applyReelMode();
            }
            break;
        // Reel detents only reach here when idle (playback spins the reel instead)
        case InputEvent::ENC_CW:
            gPlayer->setVolume(gPlayer->getVolume() + 0.03f * msg.encoderDelta);
            break;
        case InputEvent::ENC_CCW:
            gPlayer->setVolume(gPlayer->getVolume() - 0.03f * msg.encoderDelta);
            break;
        case InputEvent::ENC_PRESS:
            st.screen = Screen::EQ;
            st.eqBand = 0;
            applyReelMode();
            break;
        default: break;
    }
}

static void openSelected() {
    if (st.menuIndex >= fileCount) return;
    if (fileList[st.menuIndex][0] == '[') {
        strncpy(currentDir, filePaths[st.menuIndex], sizeof(currentDir) - 1);
        st.menuIndex = 0;
        st.menuScroll = 0;
        scanDir(currentDir);
    } else {
        gPlayer->play(filePaths[st.menuIndex]);
        st.screen = Screen::NOW_PLAYING;
        st.scrollOffset = 0;
        applyReelMode();
    }
}

static void inputFileBrowser(const InputMsg& msg) {
    switch (msg.event) {
        case InputEvent::ENC_CW:
            st.menuIndex = min(st.menuIndex + (int)msg.encoderDelta, fileCount - 1);
            if (st.menuIndex >= st.menuScroll + 5) st.menuScroll = st.menuIndex - 4;
            break;
        case InputEvent::ENC_CCW:
            st.menuIndex = max(st.menuIndex - (int)msg.encoderDelta, 0);
            if (st.menuIndex < st.menuScroll) st.menuScroll = st.menuIndex;
            break;
        case InputEvent::ENC_PRESS:
            openSelected();
            break;
        case InputEvent::BTN_PRESS:
            if (msg.button == Button::PLAY) {
                openSelected();
            } else if (msg.button == Button::BACK) {
                if (strcmp(currentDir, "/") != 0) {
                    char* last = strrchr(currentDir, '/');
                    if (last && last != currentDir) {
                        *last = '\0';
                        char* p = strrchr(currentDir, '/');
                        if (p) *(p + 1) = '\0';
                        else strcpy(currentDir, "/");
                    } else {
                        strcpy(currentDir, "/");
                    }
                    st.menuIndex = 0;
                    st.menuScroll = 0;
                    scanDir(currentDir);
                } else {
                    st.screen = Screen::NOW_PLAYING;
                    applyReelMode();
                }
            }
            break;
        default: break;
    }
}

static void inputEQ(const InputMsg& msg) {
    switch (msg.event) {
        case InputEvent::BTN_PRESS:
            if (msg.button == Button::ROCKER_UP) {
                st.eqBand = min(st.eqBand + 1, EQ_BANDS - 1);
            } else if (msg.button == Button::ROCKER_DN) {
                st.eqBand = max(st.eqBand - 1, 0);
            } else if (msg.button == Button::BACK) {
                st.screen = Screen::NOW_PLAYING;
                applyReelMode();
            }
            break;
        case InputEvent::ENC_CW:
            gEq->setGain(st.eqBand, gEq->getGain(st.eqBand) + 1.0f * msg.encoderDelta);
            break;
        case InputEvent::ENC_CCW:
            gEq->setGain(st.eqBand, gEq->getGain(st.eqBand) - 1.0f * msg.encoderDelta);
            break;
        case InputEvent::ENC_LONG_PRESS:
            gEq->reset();
            break;
        default: break;
    }
}

static void inputSettings(const InputMsg& msg) {
    switch (msg.event) {
        case InputEvent::ENC_CW:
            st.menuIndex = min(st.menuIndex + (int)msg.encoderDelta, 4);
            break;
        case InputEvent::ENC_CCW:
            st.menuIndex = max(st.menuIndex - (int)msg.encoderDelta, 0);
            break;
        case InputEvent::ENC_PRESS:
        case InputEvent::BTN_PRESS:
            if (st.menuIndex == 4 || (msg.event == InputEvent::BTN_PRESS && msg.button == Button::BACK)) {
                st.screen = Screen::NOW_PLAYING;
                applyReelMode();
            }
            // ponytail: WiFi/BT/Preset submenus when those systems exist
            break;
        default: break;
    }
}

// ── Public API ─────────────────────────────────────────
void UI::init(AudioPlayer* player, Equalizer* eq) {
    // Allocate file browser arrays in PSRAM (saves ~40KB SRAM)
    if (!fileList)  fileList  = (char(*)[64])ps_calloc(MAX_FILES, 64);
    if (!filePaths) filePaths = (char(*)[256])ps_calloc(MAX_FILES, 256);
    if (!fileList || !filePaths) {
        Serial.println("[UI] PSRAM alloc failed, falling back to heap");
        if (!fileList)  fileList  = (char(*)[64])calloc(MAX_FILES, 64);
        if (!filePaths) filePaths = (char(*)[256])calloc(MAX_FILES, 256);
    }
    gPlayer = player;
    gEq = eq;
    st.screen = Screen::SPLASH;
    st.splashEnd = millis() + 1500;
    Serial.println("[UI] Initialized");
}

void UI::handleInput(const InputMsg& msg) {
    markDirty();
    switch (st.screen) {
        case Screen::NOW_PLAYING:  inputNowPlaying(msg); break;
        case Screen::FILE_BROWSER: inputFileBrowser(msg); break;
        case Screen::EQ:           inputEQ(msg); break;
        case Screen::SETTINGS:     inputSettings(msg); break;
        case Screen::SPLASH:
            st.screen = Screen::NOW_PLAYING;
            applyReelMode();
            break;
    }
}

void UI::render() {
    if (st.screen == Screen::SPLASH && millis() > st.splashEnd) {
        st.screen = Screen::NOW_PLAYING;
        applyReelMode();
        markDirty();
    }

    // A moving reel means the artwork changes every frame
    if (fabsf(ReelMotor::getVelocity()) > 0.05f) dirty = true;
    if (st.screen == Screen::NOW_PLAYING && gPlayer && gPlayer->isPlaying()) dirty = true;

    if (!dirty) return;
    dirty = false;

    LGFX_Sprite& g = Display::canvas();
    g.fillScreen(C_BG);

    switch (st.screen) {
        case Screen::SPLASH:       renderSplash(g); break;
        case Screen::NOW_PLAYING:  renderNowPlaying(g); break;
        case Screen::FILE_BROWSER: renderFileBrowser(g); break;
        case Screen::EQ:           renderEQ(g); break;
        case Screen::SETTINGS:     renderSettings(g); break;
    }

    Display::flush();
}

void UI::taskFunc(void* param) {
    TickType_t lastWake = xTaskGetTickCount();
    while (true) {
        InputMsg msg;
        while (Input::poll(msg)) {
            handleInput(msg);
        }
        render();
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(33));  // ~30fps
    }
}

UIState& UI::getState() { return st; }
void UI::setScreen(Screen s) {
    st.screen = s; st.menuIndex = 0; st.menuScroll = 0;
    applyReelMode(); markDirty();
}
void UI::setBattery(float pct)      { if (st.batteryPct != pct) markDirty(); st.batteryPct = pct; }
void UI::setBtConnected(bool c)     { if (st.btConnected != c) markDirty(); st.btConnected = c; }
void UI::setWifiConnected(bool c)   { if (st.wifiConnected != c) markDirty(); st.wifiConnected = c; }