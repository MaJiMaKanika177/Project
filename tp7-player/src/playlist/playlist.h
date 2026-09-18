#pragma once
#include <cstdint>

enum class PlayMode : uint8_t {
    SEQUENTIAL, SHUFFLE, REPEAT_ONE, REPEAT_ALL
};

class Playlist {
public:
    bool init();

    int scanSD(const char* rootDir = "/");

    bool loadM3U(const char* path);
    bool saveM3U(const char* path);

    const char* current();
    const char* next();
    const char* prev();
    const char* getTrack(int index);
    int getIndex();
    int getCount();
    void setIndex(int idx);

    void setMode(PlayMode m);
    PlayMode getMode();

    bool toggleFavorite(const char* path);
    bool isFavorite(const char* path);
    int getFavoriteCount();

    void saveResumePoint(const char* path, uint32_t position);
    bool getResumePoint(const char* path, uint32_t& position);

private:
    static constexpr int MAX_TRACKS = 512;
    static constexpr int MAX_FAVS = 64;
    static constexpr int MAX_RESUME = 50;

    // Allocated in PSRAM via init() to avoid 146KB in .bss
    char (*tracks)[256] = nullptr;  // MAX_TRACKS × 256
    int trackCount = 0;
    int currentIndex = -1;
    PlayMode mode = PlayMode::SEQUENTIAL;

    int* shuffleOrder = nullptr;  // MAX_TRACKS
    int shufflePos = 0;
    void generateShuffle();

    char (*favs)[256] = nullptr;  // MAX_FAVS × 256
    int favCount = 0;
    void loadFavorites();
    void saveFavorites();

    bool isAudioFile(const char* name);
    void scanDirRecursive(const char* dir, int depth);
};