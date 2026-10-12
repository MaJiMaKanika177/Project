#include "http_player.h"
#include <cstring>

bool HttpPlayer::init(AudioOutputI2S* output) {
    out = output;
    return out != nullptr;
}

void HttpPlayer::cleanup() {
    if (gen) { gen->stop(); delete gen; gen = nullptr; }
    if (buf) { delete buf; buf = nullptr; }
    if (http) { delete http; http = nullptr; }
    playing = false;
}

bool HttpPlayer::play(const char* url) {
    cleanup();

    http = new AudioFileSourceHTTPStream(url);
    if (!http->isOpen()) {
        Serial.printf("[HTTP] Can't open stream: %s\n", url);
        delete http; http = nullptr;
        return false;
    }

    // 8KB buffer for network jitter
    buf = new AudioFileSourceBuffer(http, 8192);

    // ponytail: assume MP3 stream, add content-type detection if needed
    gen = new AudioGeneratorMP3();

    if (!gen->begin(buf, out)) {
        Serial.printf("[HTTP] Failed to begin stream: %s\n", url);
        cleanup();
        return false;
    }

    strncpy(streamUrl, url, sizeof(streamUrl) - 1);
    playing = true;
    Serial.printf("[HTTP] Streaming: %s\n", url);
    return true;
}

void HttpPlayer::stop() {
    cleanup();
    streamUrl[0] = '\0';
    Serial.println("[HTTP] Stopped");
}

void HttpPlayer::loop() {
    if (!playing || !gen) return;
    if (gen->isRunning()) {
        if (!gen->loop()) {
            cleanup();
            Serial.println("[HTTP] Stream ended");
        }
    }
}

bool HttpPlayer::isPlaying() { return playing; }
const char* HttpPlayer::getStreamUrl() { return streamUrl; }