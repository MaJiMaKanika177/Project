#include <Arduino.h>
#include <Wire.h>
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
void setup() {
    Serial.begin(115200);

    // USB CDC on ESP32-S3 needs time to re-enumerate after reset.
    // Without this delay, early Serial.print() messages are lost because
    // the host hasn't opened the port yet.
    delay(2000);

    Serial.printf("\n%s v%s booting...\n", DEVICE_NAME, FW_VERSION);

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    Display::init();
    SDCard::init();

    eq.init(SAMPLE_RATE);
    player.init(&eq);
    vuMeter.init();
    bool reelOk = ReelMotor::init();
    Battery::init();

    Settings::init();
    Settings::load(&player, &eq);

    playlist.init();
    if (SDCard::isReady()) playlist.scanSD("/");

    // WiFi + web UI. NOTE: the web server and its /update OTA endpoint have no
    // authentication — anyone on the same network can control the player and
    // flash firmware. Add ElegantOTA.setAuth() before using outside home LAN.
    WiFiMgr::init();
    WebServer::setPlayerRef(&player);
    WebServer::setEqRef(&eq);
    WebServer::init();

    Input::init();
    UI::init(&player, &eq);

    xTaskCreatePinnedToCore(Input::taskFunc, "input", 4096, NULL, 4, NULL, 0);
    xTaskCreatePinnedToCore(UI::taskFunc,    "ui",    8192, NULL, 3, NULL, 0);
    xTaskCreatePinnedToCore(houseTask,       "house", 4096, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(audioTask,       "audio", 8192, NULL, 5, NULL, 1);
    // FOC needs a tight loop; core 1 alongside audio, one priority below it.
    if (reelOk) {
        xTaskCreatePinnedToCore(ReelMotor::focTask, "foc", 4096, NULL, 4, NULL, 1);
    }

    Serial.printf("[Boot] Ready — %d tracks, http://%s/\n",
                  playlist.getCount(), WiFiMgr::getIP().c_str());
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}