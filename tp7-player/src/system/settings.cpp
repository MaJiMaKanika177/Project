#include "settings.h"
#include "../audio/player.h"
#include "../audio/eq.h"
#include "config.h"
#include <Arduino.h>
#include <Preferences.h>

// Preferences (NVS) instead of LittleFS+JSON — 20 scalars don't need a
// filesystem or a JSON parser.
static Preferences prefs;
static bool dirty = false;
static uint32_t dirtySince = 0;

bool Settings::init() {
    if (!prefs.begin("tp7", false)) {
        Serial.println("[Settings] NVS open failed");
        return false;
    }
    return true;
}

void Settings::load(AudioPlayer* player, Equalizer* eq) {
    if (player) player->setVolume(prefs.getFloat("vol", 0.5f));
    if (eq) {
        char key[8];
        for (int i = 0; i < EQ_BANDS; i++) {
            snprintf(key, sizeof(key), "eq%d", i);
            eq->setGain(i, prefs.getFloat(key, 0.0f));
        }
    }
    Serial.println("[Settings] Loaded");
}

void Settings::save(AudioPlayer* player, Equalizer* eq) {
    if (player) prefs.putFloat("vol", player->getVolume());
    if (eq) {
        char key[8];
        for (int i = 0; i < EQ_BANDS; i++) {
            snprintf(key, sizeof(key), "eq%d", i);
            prefs.putFloat(key, eq->getGain(i));
        }
    }
    dirty = false;
    Serial.println("[Settings] Saved");
}

void Settings::markDirty() {
    if (!dirty) dirtySince = millis();
    dirty = true;
}

void Settings::tick(AudioPlayer* player, Equalizer* eq) {
    // Debounce: write 3s after the last change, so a volume sweep is one write
    if (dirty && millis() - dirtySince > 3000) {
        save(player, eq);
    }
}