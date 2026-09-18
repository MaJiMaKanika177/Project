#include "wifi_manager.h"
#include "config.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <Preferences.h>

static Preferences prefs;
static bool apMode = false;
static DNSServer dnsServer;
static bool dnsRunning = false;

void WiFiMgr::init() {
    prefs.begin("wifi", false);
    String ssid = prefs.getString("ssid", "");
    String pass = prefs.getString("pass", "");

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    if (ssid.length() > 0) {
        Serial.printf("[WiFi] Trying stored SSID: %s\n", ssid.c_str());
        if (connectSTA(ssid.c_str(), pass.c_str())) return;
    }
    Serial.println("[WiFi] No creds / connect failed → AP mode");
    startAP();
}

bool WiFiMgr::connectSTA(const char* ssid, const char* pass, uint32_t timeoutMs) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
        delay(200);
    }

    if (WiFi.status() == WL_CONNECTED) {
        apMode = false;
        prefs.putString("ssid", ssid);
        prefs.putString("pass", pass);
        Serial.printf("[WiFi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());
        return true;
    }
    Serial.println("[WiFi] Connect timeout");
    return false;
}

void WiFiMgr::startAP() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);

    // Configure AP with static IP and proper subnet
    IPAddress apIP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(apIP, gateway, subnet);

    // Captive portal DNS: redirect ALL domain lookups to our IP.
    // This prevents OS from deciding "no internet" and refusing connections.
    dnsServer.start(53, "*", apIP);
    dnsRunning = true;

    apMode = true;
    Serial.printf("[WiFi] AP started: %s / IP: %s (captive DNS active)\n", AP_SSID, WiFi.softAPIP().toString().c_str());
}

void WiFiMgr::stop() {
    if (dnsRunning) { dnsServer.stop(); dnsRunning = false; }
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    apMode = false;
    Serial.println("[WiFi] Off");
}

void WiFiMgr::loop() {
    if (dnsRunning) dnsServer.processNextRequest();
}

bool WiFiMgr::isConnected() { return WiFi.status() == WL_CONNECTED; }
bool WiFiMgr::isAP() { return apMode; }

String WiFiMgr::getIP() {
    return apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
}

String WiFiMgr::scanNetworksJSON() {
    int n = WiFi.scanNetworks();
    String json;
    json.reserve(1024);
    json = "[";
    for (int i = 0; i < n; i++) {
        if (i) json += ",";
        char entry[128];
        snprintf(entry, sizeof(entry),
            "{\"ssid\":\"%s\",\"rssi\":%d,\"enc\":%d}",
            WiFi.SSID(i).c_str(), WiFi.RSSI(i),
            WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? 1 : 0);
        json += entry;
    }
    json += "]";
    WiFi.scanDelete();
    return json;
}