#include "player.h"
#include "eq_output.h"
#include "pins.h"
#include "config.h"
#include "util.h"
#include "../system/settings.h"
#include <cstring>

AudioFormat AudioPlayer::detectFormat(const char* path) {
    return audioFormatFromPath(path);
}

bool AudioPlayer::init(Equalizer* eq) {
    mtx = xSemaphoreCreateMutex();
    eqRef = eq;
    out = new AudioOutputI2S();
    out->SetPinout(PIN_I2S_BCK, PIN_I2S_WS, PIN_I2S_DOUT);
    out->SetGain(volume);
    if (eqRef) {
        eqOut = new AudioOutputEQ(out, eqRef);
    }
    Serial.println("[Player] Initialized");
    return true;
}

void AudioPlayer::cleanup() {
    // no lock here — callers hold it
    if (gen) { gen->stop(); delete gen; gen = nullptr; }
    if (buf) { delete buf; buf = nullptr; }
    if (src) { src->close(); delete src; src = nullptr; }
    state = PlayState::STOPPED;
}

bool AudioPlayer::play(const char* path) {
    lock();
    cleanup();

    AudioFormat fmt = detectFormat(path);
    if (fmt == AudioFormat::UNKNOWN) {
        Serial.printf("[Player] Unknown format: %s\n", path);
        unlock();
        return false;
    }

    src = new AudioFileSourceSD(path);
    if (!src->isOpen()) {
        Serial.printf("[Player] Can't open: %s\n", path);
        delete src; src = nullptr;
        unlock();
        return false;
    }

    buf = new AudioFileSourceBuffer(src, 2048);

    switch (fmt) {
        case AudioFormat::MP3:  gen = new AudioGeneratorMP3();  break;
        case AudioFormat::WAV:  gen = new AudioGeneratorWAV();  break;
        case AudioFormat::FLAC: gen = new AudioGeneratorFLAC(); break;
        default: cleanup(); unlock(); return false;
    }

    out->SetGain(volume);
    AudioOutput* dest = eqOut ? (AudioOutput*)eqOut : (AudioOutput*)out;
    if (!gen->begin(buf, dest)) {
        Serial.printf("[Player] Failed to begin: %s\n", path);
        cleanup();
        unlock();
        return false;
    }

    strncpy(currentTrack, path, sizeof(currentTrack) - 1);
    state = PlayState::PLAYING;
    Serial.printf("[Player] Playing: %s\n", path);
    unlock();
    return true;
}

void AudioPlayer::stop() {
    lock();
    cleanup();
    currentTrack[0] = '\0';
    Serial.println("[Player] Stopped");
    unlock();
}

void AudioPlayer::pause() {
    lock();
    if (state == PlayState::PLAYING) {
        state = PlayState::PAUSED;
        Serial.println("[Player] Paused");
    }
    unlock();
}

void AudioPlayer::resume() {
    lock();
    if (state == PlayState::PAUSED) {
        state = PlayState::PLAYING;
        Serial.println("[Player] Resumed");
    }
    unlock();
}

void AudioPlayer::loop() {
    lock();
    if (state != PlayState::PLAYING || !gen) { unlock(); return; }
    if (gen->isRunning()) {
        if (!gen->loop()) {
            cleanup();
            Serial.println("[Player] Track ended");
        }
    }
    unlock();
}

bool AudioPlayer::isPlaying() { lock(); bool r = state == PlayState::PLAYING; unlock(); return r; }
PlayState AudioPlayer::getState() { lock(); PlayState s = state; unlock(); return s; }

float AudioPlayer::getProgress() {
    lock();
    float r = 0;
    if (src) {
        uint32_t pos = src->getPos();
        uint32_t size = src->getSize();
        r = size > 0 ? (float)pos / size : 0;
    }
    unlock();
    return r;
}

void AudioPlayer::setVolume(float vol) {
    lock();
    volume = constrain(vol, 0.0f, 1.0f);
    if (out) out->SetGain(volume);
    unlock();
    Settings::markDirty();
}

float AudioPlayer::getVolume() { lock(); float v = volume; unlock(); return v; }

const char* AudioPlayer::getCurrentTrack() { return currentTrack; }
// ponytail: getCurrentTrack returns pointer to internal buffer — safe because
// strncpy is only called under lock and readers just read; for full safety
// copy into caller buffer