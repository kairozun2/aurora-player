// Aurora Player - shared value types for the audio core.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace aurora {

enum class PlaybackState { Stopped, Playing, Paused, Buffering, Error };
enum class RepeatMode { Off, All, One };
enum class SourceKind { LocalFile, Downloaded, Stream };

inline const char* toString(PlaybackState s) {
    switch (s) {
        case PlaybackState::Stopped: return "stopped";
        case PlaybackState::Playing: return "playing";
        case PlaybackState::Paused: return "paused";
        case PlaybackState::Buffering: return "buffering";
        case PlaybackState::Error: return "error";
    }
    return "unknown";
}

inline const char* toString(RepeatMode m) {
    switch (m) {
        case RepeatMode::Off: return "off";
        case RepeatMode::All: return "all";
        case RepeatMode::One: return "one";
    }
    return "off";
}

inline RepeatMode repeatFromString(const std::string& s) {
    if (s == "all") return RepeatMode::All;
    if (s == "one") return RepeatMode::One;
    return RepeatMode::Off;
}

/// A single playable item. `path` is a local file path or an http(s) stream URL.
struct Track {
    std::string id;            ///< stable 64-bit FNV hash of the canonical path
    std::string path;          ///< local absolute path or stream URL
    std::string title;
    std::string artist;
    std::string album;
    std::string albumArtist;
    std::string genre;
    std::string year;
    int trackNo = 0;
    int discNo = 0;
    double durationSec = 0.0;
    int sampleRate = 0;
    int channels = 0;
    int bitrateKbps = 0;
    std::uint64_t fileSize = 0;
    std::int64_t mtime = 0;        ///< file modification time (unix seconds)
    std::int64_t addedAt = 0;      ///< when the track entered the library
    std::int64_t lastPlayedAt = 0;
    int playCount = 0;
    bool favorite = false;
    int rating = 0;                ///< 0..5
    bool hasEmbeddedCover = false;
    SourceKind source = SourceKind::LocalFile;
    std::string sourceUrl;         ///< original web URL when downloaded
    std::string coverPath;         ///< sidecar/extracted cover image, if any
    std::string lyricsPath;        ///< sidecar .lrc/.txt, if any

    bool valid() const { return !path.empty(); }
    bool isStream() const { return source == SourceKind::Stream; }
    const std::string& displayTitle() const { return title.empty() ? path : title; }
    /// Artist, falling back to the album artist. Empty when unknown.
    std::string displayArtist() const {
        if (!artist.empty()) return artist;
        return albumArtist;
    }
};

/// Immutable snapshot of the player, safe to copy across threads.
/// The engine fills the playback fields; the controller adds the queue context.
struct PlayerSnapshot {
    PlaybackState state = PlaybackState::Stopped;
    std::string uri;
    double positionSec = 0.0;
    double durationSec = 0.0;
    float volume = 1.0f;
    bool muted = false;
    float speed = 1.0f;
    bool crossfading = false;

    // Filled in by Controller::snapshot()
    Track track;
    RepeatMode repeat = RepeatMode::Off;
    bool shuffle = false;
};

/// Lightweight performance counters, updated from the render thread.
struct EngineMetrics {
    std::uint64_t framesRendered = 0;
    std::uint64_t underruns = 0;
    std::uint64_t decodedFrames = 0;
    double renderCpuMsPerSecond = 0.0;  ///< DSP cost: ms of CPU per second of audio
    double bufferFill = 0.0;            ///< 0..1 ring buffer occupancy
};

struct AudioFormat {
    int sampleRate = 48000;
    int channels = 2;
    bool operator==(const AudioFormat& o) const {
        return sampleRate == o.sampleRate && channels == o.channels;
    }
};

} // namespace aurora
