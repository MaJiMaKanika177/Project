#include "web_server.h"
#include "wifi_manager.h"
#include "config.h"
#include "util.h"
#include "../audio/player.h"
#include "../audio/eq.h"
#include "errorlog.h"
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

static bool extOk(const String& name, const char* exts) {
    int dot = name.lastIndexOf('.');
    if (dot < 0) return false;
    String e = name.substring(dot);
    e.toLowerCase();
    return e == exts;
}

static bool isAcceptedUpload(const String& name) {
    return extOk(name, ".mp3") || extOk(name, ".wav") || extOk(name, ".flac")
        || extOk(name, ".jpg") || extOk(name, ".jpeg") || extOk(name, ".png");
}

// Single-page web app: player + library + upload. Mobile-first, dark TP7 theme.
// Talks to /api/* endpoints; served at / and /upload.
static const char APP_HTML[] PROGMEM = R"rawliteral(<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0d0d0d">
<title>TP7-Player</title><style>
:root{--bg:#0a0a0a;--card:#161616;--line:#242424;--fg:#eee;--mut:#8a8a8a;--acc:#ff9500;--ok:#4caf50;--bad:#f44336}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{background:var(--bg);color:var(--fg);font:15px system-ui,sans-serif;max-width:640px;margin:0 auto;padding:12px calc(12px + env(safe-area-inset-right)) calc(24px + env(safe-area-inset-bottom)) calc(12px + env(safe-area-inset-left))}
h1{font-size:17px;color:var(--acc);margin:6px 2px 14px;display:flex;align-items:center;gap:8px}
h1 .dot{width:10px;height:10px;border-radius:50%;background:var(--acc);box-shadow:0 0 12px var(--acc)}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:14px;margin-bottom:12px}
.row{display:flex;gap:10px;align-items:center}
button{background:#222;color:var(--fg);border:1px solid var(--line);border-radius:10px;padding:10px 16px;cursor:pointer;font-size:15px;touch-action:manipulation}
button:active{transform:scale(.96)}
button.acc{background:var(--acc);color:#111;border:0;font-weight:700}
button.big{width:52px;height:52px;border-radius:50%;font-size:18px;display:grid;place-items:center;padding:0}
input[type=text]{background:#101010;border:1px solid var(--line);color:var(--fg);border-radius:10px;padding:10px;width:100%;font-size:15px}
input[type=file]{color:var(--mut);font-size:13px;width:100%;margin:4px 0 10px}
input[type=range]{flex:1;accent-color:var(--acc);height:32px}
.now{display:flex;gap:14px;align-items:center}
.cover{width:88px;height:88px;border-radius:12px;background:#000 center/cover no-repeat;flex:none;border:1px solid var(--line)}
.cover.def{background-image:url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'%3E%3Ccircle cx='50' cy='50' r='46' fill='%23111' stroke='%23333'/%3E%3Ccircle cx='50' cy='50' r='14' fill='%23ff9500'/%3E%3Ccircle cx='50' cy='50' r='4' fill='%23111'/%3E%3C/svg%3E")}
.meta{min-width:0;flex:1}
#track{font-size:15px;font-weight:600;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
#state{color:var(--mut);font-size:12px;margin-top:2px;text-transform:uppercase;letter-spacing:.5px}
#bar{height:6px;background:#222;border-radius:3px;margin-top:12px;overflow:hidden;cursor:pointer}
#fill{height:100%;width:0;background:var(--acc);border-radius:3px;transition:width .5s linear}
#err{color:var(--bad);font-size:12px;margin-top:8px;word-break:break-word}
.frow{display:flex;justify-content:space-between;align-items:center;padding:11px 6px;border-bottom:1px solid #1d1d1d;gap:10px;min-height:44px}
.frow:last-child{border:0}
.fn{overflow:hidden;text-overflow:ellipsis;white-space:nowrap;flex:1}
a{color:var(--acc);text-decoration:none;cursor:pointer}
.hint{color:var(--mut);font-size:12px}
label.hint{display:block;margin:10px 0 2px}
.dirnav{display:flex;gap:8px;margin:8px 0;align-items:center}
#dir{color:var(--mut);font-size:12px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.empty{padding:14px 6px}
@media(max-width:380px){.cover{width:72px;height:72px}button.big{width:46px;height:46px}}
</style></head><body>
<h1><span class="dot"></span>TP7-Player</h1>

<div class="card">
 <div class="now">
  <div class="cover def" id="cover"></div>
  <div class="meta"><div id="track">No track</div><div id="state">stopped</div>
   <div class="row" style="margin-top:8px">
    <button class="big" onclick="prev()">&#9198;</button>
    <button class="big acc" id="pp" onclick="cmd('/api/pause')">&#9208;</button>
    <button class="big" onclick="next()">&#9197;</button>
   </div></div>
 </div>
 <div id="bar"><div id="fill"></div></div>
 <div class="row" style="margin-top:8px"><span class="hint">Vol</span>
  <input type="range" id="vol" min="0" max="100" onchange="cmd('/api/volume','vol='+this.value)"></div>
 <div id="err"></div>
</div>

<div class="card"><b>Library</b>
 <div class="dirnav"><button onclick="parent()">&larr;</button><button onclick="loadDir('/')">/</button><span id="dir"></span></div>
 <div id="files"></div>
</div>

<div class="card"><b>Upload</b>
 <label class="hint">Folder tujuan</label><input type="text" id="dirin" value="/Music/">
 <label class="hint">Lagu (.mp3 / .wav / .flac)</label><input type="file" id="mus" accept=".mp3,.wav,.flac" multiple>
 <label class="hint">Cover album (.jpg/.png) — nama sama dengan lagu, atau <code>cover.jpg</code></label>
 <input type="file" id="cov" accept=".jpg,.jpeg,.png" multiple>
 <div class="row" style="margin-top:10px"><button class="acc" onclick="go()">Upload</button><span id="st" class="hint"></span></div>
</div>

<script>
const $=id=>document.getElementById(id);
let cur='/Music/',playing='',listPaths=[];
function cmd(u,b){return fetch(u,{method:'POST',body:b||''}).then(poll)}
function esc(s){return s.replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]))}
function setCover(path){
 const c=$('cover');
 if(!path){c.className='cover def';c.style.backgroundImage='';return}
 c.className='cover';
 c.style.backgroundImage='url("/api/cover?path='+encodeURIComponent(path)+'")';
 c.onerror=()=>{c.className='cover def'};
}
async function poll(){try{
 const r=await(await fetch('/api/status')).json();
 if(r.track!==playing){playing=r.track;setCover(r.track||'')}
 $('state').textContent=r.state+(r.track?' \u00b7 '+(r.state==='playing'?'\u25b6':'\u23f8'):'');
 $('pp').innerHTML=r.state==='playing'?'&#9208;':'&#9654;';
 $('fill').style.width=(r.progress*100)+'%';$('vol').value=r.volume;
 const nm=r.track?r.track.split('/').pop():'No track';
 $('track').textContent=nm;$('track').title=nm;
 const e=await(await fetch('/api/errors')).json();
 const er=e.entries.filter(x=>x.startsWith('E '));
 $('err').textContent=er.length?('\u26a0 '+er[er.length-1]):'';
}catch{}}
async function loadDir(d){cur=d;$('dir').textContent=d;$('files').innerHTML='<div class="empty hint">loading\u2026</div>';
 let list=[];
 try{list=await(await fetch('/api/files?dir='+encodeURIComponent(d))).json()}catch{list=[]}
 list.sort((a,b)=>(a.dir===b.dir)?a.name.localeCompare(b.name):a.dir?-1:1);
 listPaths=list.map(f=>(d.endsWith('/')?d:d+'/')+f.name);
 $('files').innerHTML=list.map((f,i)=>f.dir
  ?'<div class="frow"><a onclick="loadDir(\''+listPaths[i].replace(/'/g,"\\'")+'\')">&#128193; '+esc(f.name)+'</a></div>'
  :'<div class="frow"><span class="fn">'+esc(f.name)+'</span><button class="big" style="width:40px;height:40px" onclick="play('+i+')">&#9654;</button></div>'
  ).join('')||'<div class="empty hint">folder kosong</div>';
}
function play(i){cmd('/api/play','path='+encodeURIComponent(listPaths[i]))}
function parent(){loadDir(cur.replace(/\/+$/,'').replace(/[^/]*$/,'')||'/')}
function trackIdx(){return listPaths.indexOf(playing)}
function prev(){const n=listPaths.length;if(!n||trackIdx()<0)return;play((trackIdx()+n-1)%n)}
function next(){const n=listPaths.length;if(!n||trackIdx()<0)return;play((trackIdx()+1)%n)}
$('bar').onclick=async ev=>{try{const r=await(await fetch('/api/status')).json();
 if(!r.track)return;/* seek belum ada API — refresh saja */}catch{}};
async function go(){$('st').textContent='mengupload\u2026';$('st').style.color='var(--mut)';
 const fd=new FormData();fd.append('dir',$('dirin').value);
 for(const f of $('mus').files)fd.append('file',f);
 for(const f of $('cov').files)fd.append('file',f);
 if(!$('mus').files.length&&!$('cov').files.length){$('st').textContent='pilih file dulu';return}
 try{const r=await fetch('/upload',{method:'POST',body:fd}),t=await r.text();
 $('st').textContent=t;$('st').style.color=r.ok?'var(--ok)':'var(--bad)';
 if(r.ok){$('mus').value='';$('cov').value='';loadDir(cur);poll()}}
 catch(e){$('st').textContent='gagal: '+e;$('st').style.color='var(--bad)'}}
setInterval(poll,2000);document.addEventListener('visibilitychange',()=>{if(!document.hidden)poll()});
loadDir(cur);poll();
</script></body></html>)rawliteral";

void WebServer::init() {
    // ponytail: SPIFFS.begin(true) removed — all HTML is served inline,
    // no static files needed. SPIFFS first-format blocks 10-30s → Task WDT.
    // Add LittleFS if static assets are needed later.

    // Root + /upload → the single-page app
    server.on("/", WEB_GET, [](AsyncWebServerRequest* req) {
        auto* res = req->beginResponse_P(200, "text/html", APP_HTML);
        addCORS(res);
        req->send(res);
    });
    server.on("/upload", WEB_GET, [](AsyncWebServerRequest* req) {
        auto* res = req->beginResponse_P(200, "text/html", APP_HTML);
        addCORS(res);
        req->send(res);
    });

    server.on("/api/errors", WEB_GET, [](AsyncWebServerRequest* req) {
        auto* res = req->beginResponse(200, "application/json", Log::ringJSON());
        addCORS(res);
        req->send(res);
    });

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

    // GET /api/cover?path=/Music/song.mp3 → sidecar image for the track.
    // Same search order as UI ensureArt: <name>.jpg/.png, <dir>/cover.jpg/.png.
    // Absent cover is normal (404, no log); unreadable existing file logs E.
    server.on("/api/cover", WEB_GET, [](AsyncWebServerRequest* req) {
        if (!req->hasParam("path")) { req->send(400, "text/plain", "no path"); return; }
        String p = req->getParam("path")->value();
        int dot = p.lastIndexOf('.');
        int slash = p.lastIndexOf('/');
        const char* cands[4];
        String a, b, c, d;
        int n = 0;
        if (dot > slash) {                       // <name>.jpg then <name>.png
            a = p.substring(0, dot) + ".jpg"; cands[n++] = a.c_str();
            b = p.substring(0, dot) + ".png"; cands[n++] = b.c_str();
        }
        if (slash >= 0) {                        // <dir>/cover.jpg then .png
            c = p.substring(0, slash) + "/cover.jpg"; cands[n++] = c.c_str();
            d = p.substring(0, slash) + "/cover.png"; cands[n++] = d.c_str();
        }
        for (int i = 0; i < n; i++) {
            File f = SD.open(cands[i]);
            if (!f) continue;
            bool isJpg = strcmp(strrchr(cands[i], '.'), ".jpg") == 0;
            const char* mime = isJpg ? "image/jpeg" : "image/png";
            auto* res = req->beginResponse(SD, cands[i], mime);
            res->addHeader("Cache-Control", "max-age=60");
            addCORS(res);
            req->send(res);
            return;
        }
        req->send(404, "text/plain", "no cover");
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
    // POST /upload  → multipart write to SD. Music: .mp3/.wav/.flac only.
    // Cover art: jpg/jpeg/png accepted (cover.* or <songname>.*).
    static File uploadFile;
    static bool uploadReject = false;

    server.on("/upload", WEB_POST,
        // onRequest — runs after all data received
        [](AsyncWebServerRequest* req) {
            auto* res = req->beginResponse(uploadReject ? 400 : 200, "text/plain",
                uploadReject ? "format ditolak — lagu: .mp3/.wav/.flac, cover: .jpg/.png (cover.jpg / nama sama dengan lagu)"
                             : "Upload selesai! Playlist auto-scan saat play berikutnya.");
            addCORS(res);
            req->send(res);
            uploadReject = false;
        },
        // onUpload — runs for each chunk of file data
        [](AsyncWebServerRequest* req, const String& filename, size_t index,
           uint8_t* data, size_t len, bool final) {
            if (!index) {
                uploadReject = false;
                if (!isAcceptedUpload(filename)) {
                    Log::warn("Upload", "rejected '%s' — extension not allowed", filename.c_str());
                    uploadReject = true;
                    return;
                }
                String dir = "/";
                if (req->hasParam("dir", true)) {
                    dir = req->getParam("dir", true)->value();
                }
                if (!dir.startsWith("/")) dir = "/" + dir;
                if (!dir.endsWith("/")) dir += "/";
                String path = dir + filename;
                Serial.printf("[Upload] Start: %s\n", path.c_str());
                uploadFile = SD.open(path.c_str(), FILE_WRITE);
                if (!uploadFile) {
                    Log::error("Upload", "can't create %s (SD full? read-only?)", path.c_str());
                    uploadReject = true;
                }
                return;
            }
            if (uploadReject) return;   // drop remaining chunks of rejected file
            if (uploadFile && len) uploadFile.write(data, len);
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