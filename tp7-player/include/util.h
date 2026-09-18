#pragma once
#include <cstring>

// Shared audio file extension check — used by playlist, UI, web server, player
inline bool isAudioFile(const char* name) {
    const char* ext = strrchr(name, '.');
    if (!ext) return false;
    return strcasecmp(ext, ".mp3") == 0 ||
           strcasecmp(ext, ".wav") == 0 ||
           strcasecmp(ext, ".flac") == 0;
}

// Extension to format enum helper
enum class AudioFormat : uint8_t { MP3, WAV, FLAC, UNKNOWN };

inline AudioFormat audioFormatFromPath(const char* path) {
    const char* ext = strrchr(path, '.');
    if (!ext) return AudioFormat::UNKNOWN;
    if (strcasecmp(ext, ".mp3") == 0)  return AudioFormat::MP3;
    if (strcasecmp(ext, ".wav") == 0)  return AudioFormat::WAV;
    if (strcasecmp(ext, ".flac") == 0) return AudioFormat::FLAC;
    return AudioFormat::UNKNOWN;
}