#pragma once
#include <cstdint>

class Equalizer;
class AudioPlayer;

namespace Settings {
    bool init();
    void load(AudioPlayer* player, Equalizer* eq);
    void save(AudioPlayer* player, Equalizer* eq);
    void markDirty();     // schedule a debounced save
    void tick(AudioPlayer* player, Equalizer* eq);  // call ~1Hz; saves if dirty
}