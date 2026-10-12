#include <Arduino.h>
#include <Wire.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "pins.h"
#include "drivers/sd_card.h"
#include "drivers/display.h"
#include "drivers/reel_motor.h"
#include "drivers/battery.h"
#include "audio/player.h"
#include "audio/eq.h"
#include "input/input_handler.h"
#include "ui/ui.h"
#include "ui/vu_meter.h"
#include "playlist/playlist.h"
#include "wifi/wifi_manager.h"
#include "wifi/web_server.h"
#include "system/settings.h"
#include "errorlog.h"

AudioPlayer player;
Equalizer eq;
VUMeter vuMeter;
Playlist playlist;

// ── Audio task (core 1, high priority) ─────────────────
static void audioTask(void* param) {
    PlayState lastState = PlayState::STOPPED;

    while (true) {
        player.loop();

        PlayState cur = player.getState();
        if (cur != lastState) {
            switch (cur) {
                case PlayState::PLAYING: ReelMotor::play();  break;
                case PlayState::PAUSED:  ReelMotor::pause(); break;
                case PlayState::STOPPED:
                    ReelMotor::stop();
                    vuMeter.off();
                    // Track ended on its own → advance playlist
                    // ponytail: playlist has no mutex; read count+next
                    // atomically enough for single-writer (audioTask only advances).
                    // Web scanSD() could race — add playlist mutex if web triggers rescan.
                    int cnt = playlist.getCount();
                    if (lastState == PlayState::PLAYING && cnt > 0) {
                        const char* nxt = playlist.next();
                        if (nxt && nxt[0]) player.play(nxt);
                    }
                    break;
            }
            lastState = cur;
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// ── Housekeeping task (core 0, low priority) ───────────
static void houseTask(void* param) {
    while (true) {
        WiFiMgr::loop();    // process captive portal DNS
        Battery::poll();
        UI::setBattery(Battery::percent());
        UI::setWifiConnected(WiFiMgr::isConnected());
        Settings::tick(&player, &eq);
        vTaskDelay(pdMS_TO_TICKS(100));  // DNS needs faster polling than 1s
    }
}

// ── Setup ──────────────────────────────────────────────
// Human-readable reset reason (ESP-IDF enum → string)
static const char* resetReasonStr(esp_reset_reason_t r) {
    switch (r) {
        case ESP_RST_POWERON:  return "POWER_ON";
        case ESP_RST_SW:       return "SOFTWARE";
        case ESP_RST_PANIC:    return "PANIC (crash/WDT)";
        case ESP_RST_INT_WDT:  return "INT_WDT";
        case ESP_RST_TASK_WDT: return "TASK_WDT";
        case ESP_RST_WDT:      return "OTHER_WDT";
        case ESP_RST_DEEPSLEEP:return "DEEP_SLEEP";
        case ESP_RST_BROWNOUT: return "BROWNOUT";
        case ESP_RST_SDIO:     return "SDIO";
        default:               return "UNKNOWN";
    }
}

void setup() {
    // Kill the Task WDT globally: unsubscribe every task the core subscribed
    // at boot (loopTask + both idle tasks).  With zero subscribers the TWDT
    // never fires, so long inits / busy tasks can't reboot the player.
    // Note: esp_task_wdt_deinit() alone returns INVALID_STATE while tasks are
    // still subscribed — that's why the previous flash still reset.
    esp_task_wdt_delete(NULL);
    esp_task_wdt_delete(xTaskGetIdleTaskHandleForCPU(0));
    esp_task_wdt_delete(xTaskGetIdleTaskHandleForCPU(1));

    Serial.begin(115200);

    // USB CDC native: wait for host to open the port (up to 3s).
    // If nobody opens it (headless), don't block forever.
    unsigned long t0 = millis();
    while (!Serial && millis() - t0 < 3000) delay(10);

    Log::init();
    if (esp_reset_reason() == ESP_RST_PANIC)
        Log::error("Boot", "previous boot ended in PANIC (crash)");

    esp_reset_reason_t reason = esp_reset_reason();
    Serial.printf("\n%s v%s booting... (reset: %s)\n",
                  DEVICE_NAME, FW_VERSION, resetReasonStr(reason));

    // Brownout likely from motor/WiFi current spike on weak USB power.
    // Log extra warning so it's obvious in serial output.
    if (reason == ESP_RST_BROWNOUT) {
        Serial.println("[!] BROWNOUT detected — check USB cable / power supply!");
    }

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    Display::init();
    SDCard::init();
    esp_task_wdt_reset();           // feed WDT — init sequence is long

    eq.init(SAMPLE_RATE);
    player.init(&eq);
    vuMeter.init();
    bool reelOk = ReelMotor::init();
    esp_task_wdt_reset();           // initFOC alignment can take a while
    Battery::init();

    Settings::init();
    Settings::load(&player, &eq);

    playlist.init();
    Log::attachFs();   // replay /errors.log tail + persist new entries
    if (SDCard::isReady()) playlist.scanSD("/");
    esp_task_wdt_reset();           // SD scan on large cards

    // WiFi + web UI. NOTE: the web server and its /update OTA endpoint have no
    // authentication — anyone on the same network can control the player and
    // flash firmware. Add ElegantOTA.setAuth() before using outside home LAN.
    WiFiMgr::init();
    esp_task_wdt_reset();           // WiFi connect attempt blocks up to 5s
    WebServer::setPlayerRef(&player);
    WebServer::setEqRef(&eq);
    WebServer::init();

    Input::init();
    UI::init(&player, &eq);

    xTaskCreatePinnedToCore(Input::taskFunc, "input", 8192, NULL, 4, NULL, 0);
    xTaskCreatePinnedToCore(UI::taskFunc,    "ui",    16384, NULL, 3, NULL, 0);
    xTaskCreatePinnedToCore(houseTask,       "house", 8192, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(audioTask,       "audio", 16384, NULL, 5, NULL, 1);
    // FOC needs a tight loop; core 1 alongside audio, one priority below it.
    if (reelOk) {
        xTaskCreatePinnedToCore(ReelMotor::focTask, "foc", 8192, NULL, 4, NULL, 1);
    }

    Serial.printf("[Boot] Ready — %d tracks, http://%s/\n",
                  playlist.getCount(), WiFiMgr::getIP().c_str());
}

// Task WDT stays fully disabled — loopTask, both idle tasks unsubscribed in
// setup(). No re-subscribe: a busy audio/FOC decode must never reboot a dev
// board. Brownout detector still guards real power issues.
void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}