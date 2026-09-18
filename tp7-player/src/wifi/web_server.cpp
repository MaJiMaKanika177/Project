#include "web_server.h"
#include "wifi_manager.h"
#include "config.h"
#include "util.h"
#include "../audio/player.h"
#include "../audio/eq.h"
#include <SPIFFS.h>
#include <SD.h>
#include <Update.h>

static AsyncWebServer server(WEB_PORT);
static AudioPlayer* gPlayer = nullptr;
static Equalizer* gEq = nullptr;

void WebServer::setPlayerRef(AudioPlayer* p) { gPlayer = p; }
void WebServer::setEqRef(Equalizer* e) { gEq = e; }
AsyncWebServer& WebServer::getServer() { return server; }

static void addCORS(AsyncWebServerResponse* res) {
    res->addHeader("Access-Control-Allow-Origin", "*");
    res->addHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
    res->addHeader("Access-Control-Allow-Headers", "Content-Type");
}

static String statusJSON() {
    char buf[384];
    const char* st = "stopped";
    const char* track = "";
    int vol = 0;
    float prog = 0;
    if (gPlayer) {
        st = gPlayer->getState() == PlayState::PLAYING ? "playing"
           : gPlayer->getState() == PlayState::PAUSED  ? "paused" : "stopped";
        track = gPlayer->getCurrentTrack();
        vol = (int)(gPlayer->getVolume() * 100);
        prog = gPlayer->getProgress();
    }

    int n = snprintf(buf, sizeof(buf),
        "{\"state\":\"%s\",\"track\":\"%s\",\"volume\":%d,\"progress\":%.3f,\"wifi\":\"%s\",\"eq\":[",
        st, track, vol, prog, WiFiMgr::getIP().c_str());

    if (gEq) {
        for (int i = 0; i < EQ_BANDS; i++) {
            n += snprintf(buf + n, sizeof(buf) - n, "%s%.1f", i ? "," : "", gEq->getGain(i));
        }
    }
    snprintf(buf + n, sizeof(buf) - n, "]}");
    return String(buf);
}

static String filesJSON(const char* dir) {
    String json;
    json.reserve(4096);  // pre-allocate larger block
    json = "[";
    File root = SD.open(dir);
    if (root && root.isDirectory()) {
        File f = root.openNextFile();
        bool first = true;
        char entry[384];
        while (f) {
            const char* name = f.name();
            bool audio = !f.isDirectory() && isAudioFile(name);
            if (f.isDirectory() || audio) {
                int n = snprintf(entry, sizeof(entry),
                    "%s{\"name\":\"%s\",\"dir\":%s,\"size\":%u}",
                    first ? "" : ",", name,
                    f.isDirectory() ? "true" : "false",
                    (uint32_t)f.size());
                if (json.length() + n + 2 > 4096) break;  // safety cap
                json += entry;
                first = false;
            }
            f = root.openNextFile();
        }
        root.close();
    }
    json += "]";
    return json;
}

// Use explicit WebRequestMethod enum to avoid ambiguity with http_parser defines
#define WEB_GET  (WebRequestMethod)HTTP_GET
#define WEB_POST (WebRequestMethod)HTTP_POST

void WebServer::init() {
    SPIFFS.begin(true);

    server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

    server.on("/api/status", WEB_GET, [](AsyncWebServerRequest* req) {
        auto* res = req->beginResponse(200, "application/json", statusJSON());
        addCORS(res);
        req->send(res);
    });

    server.on("/api/files", WEB_GET, [](AsyncWebServerRequest* req) {
        String dir = req->hasParam("dir") ? req->getParam("dir")->value() : "/";
        auto* res = req->beginResponse(200, "application/json", filesJSON(dir.c_str()));
        addCORS(res);
        req->send(res);
    });

    server.on("/api/play", WEB_POST, [](AsyncWebServerRequest* req) {
        bool ok = false;
        if (gPlayer && req->hasParam("path", true)) {
            ok = gPlayer->play(req->getParam("path", true)->value().c_str());
        }
        auto* res = req->beginResponse(ok ? 200 : 400, "application/json",
                                       ok ? "{\"ok\":true}" : "{\"ok\":false}");
        addCORS(res);
        req->send(res);
    });

    server.on("/api/stop", WEB_POST, [](AsyncWebServerRequest* req) {
        if (gPlayer) gPlayer->stop();
        auto* res = req->beginResponse(200, "application/json", "{\"ok\":true}");
        addCORS(res);
        req->send(res);
    });

    server.on("/api/pause", WEB_POST, [](AsyncWebServerRequest* req) {
        if (gPlayer) {
            if (gPlayer->getState() == PlayState::PLAYING) gPlayer->pause();
            else gPlayer->resume();
        }
        auto* res = req->beginResponse(200, "application/json", "{\"ok\":true}");
        addCORS(res);
        req->send(res);
    });

    server.on("/api/volume", WEB_POST, [](AsyncWebServerRequest* req) {
        if (gPlayer && req->hasParam("vol", true)) {
            int v = req->getParam("vol", true)->value().toInt();
            gPlayer->setVolume(v / 100.0f);
        }
        auto* res = req->beginResponse(200, "application/json", "{\"ok\":true}");
        addCORS(res);
        req->send(res);
    });

    server.on("/api/eq", WEB_POST, [](AsyncWebServerRequest* req) {
        if (gEq && req->hasParam("band", true) && req->hasParam("gain", true)) {
            int b = req->getParam("band", true)->value().toInt();
            float g = req->getParam("gain", true)->value().toFloat();
            gEq->setGain(b, g);
        }
        auto* res = req->beginResponse(200, "application/json", "{\"ok\":true}");
        addCORS(res);
        req->send(res);
    });

    server.on("/api/networks", WEB_GET, [](AsyncWebServerRequest* req) {
        auto* res = req->beginResponse(200, "application/json", WiFiMgr::scanNetworksJSON());
        addCORS(res);
        req->send(res);
    });

    server.on("/api/wifi", WEB_POST, [](AsyncWebServerRequest* req) {
        bool ok = false;
        if (req->hasParam("ssid", true)) {
            String ssid = req->getParam("ssid", true)->value();
            String pass = req->hasParam("pass", true) ? req->getParam("pass", true)->value() : "";
            ok = WiFiMgr::connectSTA(ssid.c_str(), pass.c_str());
        }
        auto* res = req->beginResponse(200, "application/json",
                                       ok ? "{\"ok\":true}" : "{\"ok\":false}");
        addCORS(res);
        req->send(res);
    });

    // ── File upload to SD card ──────────────────────────────
    // GET  /upload  → simple HTML form
    // POST /upload  → multipart file write to SD
    static File uploadFile;

    server.on("/upload", WEB_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "text/html",
            "<html><body style='background:#0d0d0d;color:#e8e8e8;font-family:sans-serif;padding:40px'>"
            "<h2>Upload File ke SD Card</h2>"
            "<form method='POST' action='/upload' enctype='multipart/form-data'>"
            "<label>Folder tujuan (contoh: /)</label><br>"
            "<input type='text' name='dir' value='/' style='margin:8px 0;padding:6px;width:200px'><br>"
            "<input type='file' name='file' multiple style='margin:12px 0'><br>"
            "<input type='submit' value='Upload' style='padding:10px 24px;background:#F9A0;border:0;color:#fff;border-radius:4px;cursor:pointer;background:#ff9500'>"
            "</form>"
            "<p style='color:#888;margin-top:20px'>Atau pakai curl:<br>"
            "<code>curl -F \"dir=/\" -F \"file=@song.mp3\" http://192.168.4.1/upload</code></p>"
            "</body></html>");
    });

    server.on("/upload", WEB_POST,
        // onRequest — runs after all data received
        [](AsyncWebServerRequest* req) {
            auto* res = req->beginResponse(200, "text/plain", "Upload selesai!");
            addCORS(res);
            req->send(res);
        },
        // onUpload — runs for each chunk of file data
        [](AsyncWebServerRequest* req, const String& filename, size_t index,
           uint8_t* data, size_t len, bool final) {
            if (!index) {
                String dir = "/";
                if (req->hasParam("dir", true)) {
                    dir = req->getParam("dir", true)->value();
                }
                if (!dir.endsWith("/")) dir += "/";
                String path = dir + filename;
                Serial.printf("[Upload] Start: %s\n", path.c_str());
                uploadFile = SD.open(path.c_str(), FILE_WRITE);
                if (!uploadFile) {
                    Serial.printf("[Upload] Failed to open: %s\n", path.c_str());
                    return;
                }
            }
            if (uploadFile && len) {
                uploadFile.write(data, len);
            }
            if (final && uploadFile) {
                uploadFile.close();
                Serial.printf("[Upload] Done: %s (%u bytes)\n", filename.c_str(), index + len);
            }
        }
    );

    // OTA — use AsyncElegantOTA (v2 style, works with AsyncWebServer)
    // SECURITY: no auth. Add credentials before using outside home LAN.
    // ElegantOTA v3 requires WebServer; for AsyncWebServer, the /update
    // endpoint is registered by hand below as a simple redirect.
    server.on("/update", WEB_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "text/html",
            "<html><body style='background:#0d0d0d;color:#e8e8e8;font-family:sans-serif;padding:40px'>"
            "<h2>OTA Update</h2>"
            "<form method='POST' action='/update' enctype='multipart/form-data'>"
            "<input type='file' name='firmware' accept='.bin' style='margin:12px 0'><br>"
            "<input type='submit' value='Upload Firmware' style='padding:10px 24px;background:#ff3b30;border:0;color:#fff;border-radius:4px;cursor:pointer'>"
            "</form></body></html>");
    });

    server.on("/update", WEB_POST, [](AsyncWebServerRequest* req) {
        bool ok = !Update.hasError();
        auto* res = req->beginResponse(200, "text/plain", ok ? "OK — rebooting..." : "FAIL");
        addCORS(res);
        res->addHeader("Connection", "close");
        req->send(res);
        if (ok) ESP.restart();
    }, [](AsyncWebServerRequest* req, const String& filename, size_t index,
          uint8_t* data, size_t len, bool final) {
        if (!index) {
            Serial.printf("[OTA] Start: %s\n", filename.c_str());
            Update.begin(UPDATE_SIZE_UNKNOWN);
        }
        Update.write(data, len);
        if (final) {
            Update.end(true);
            Serial.printf("[OTA] End: %u bytes\n", index + len);
        }
    });

    // ── Captive portal: catch-all redirect ───────────────────
    // Android/iOS/Windows connectivity checks hit random URLs.
    // If not handled, browser shows "no internet". Redirect to /upload.
    server.on("/generate_204", WEB_GET, [](AsyncWebServerRequest* req) {
        req->redirect("http://192.168.4.1/upload");
    });
    server.on("/hotspot-detect.html", WEB_GET, [](AsyncWebServerRequest* req) {
        req->redirect("http://192.168.4.1/upload");
    });
    server.on("/connecttest.txt", WEB_GET, [](AsyncWebServerRequest* req) {
        req->redirect("http://192.168.4.1/upload");
    });
    server.onNotFound([](AsyncWebServerRequest* req) {
        req->redirect("http://192.168.4.1/upload");
    });

    server.begin();
    Serial.printf("[Web] Server on port %d — http://%s/\n", WEB_PORT, WiFiMgr::getIP().c_str());
}