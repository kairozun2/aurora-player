// Aurora Player - metadata reader.
//
// Native parsers (no external dependencies) for:
//   * ID3v2.2/2.3/2.4 and ID3v1 (MP3)   - including CP1251 detection so that
//     Russian tags written by old taggers are shown correctly
//   * FLAC STREAMINFO / VORBIS_COMMENT / PICTURE
//   * MP4/M4A iTunes-style atoms
//   * RIFF/WAVE LIST-INFO
//   * Ogg Vorbis / Opus comment headers
// ffprobe is used as an optional fallback to fill in whatever is still missing.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace aurora {

struct Tags {
    std::string title;
    std::string artist;
    std::string album;
    std::string albumArtist;
    std::string genre;
    std::string year;
    std::string comment;
    std::string composer;
    int trackNo = 0;
    int discNo = 0;
    double durationSec = 0.0;
    int sampleRate = 0;
    int channels = 0;
    int bitrateKbps = 0;
    bool hasCover = false;
    std::string coverMime;
    std::string parser;   ///< which backend produced the data

    bool complete() const { return !title.empty() && !artist.empty() && durationSec > 0.0; }
};

class TagReader {
public:
    /// Reads whatever is available; always returns something usable (falls back
    /// to the file name). `error` is only filled when the file cannot be read.
    static bool read(const std::string& path, Tags* out, std::string* error = nullptr);

    /// Extracts embedded cover art (APIC / PICTURE / covr) or a sidecar image.
    static bool readCover(const std::string& path,
                          std::vector<unsigned char>* data,
                          std::string* mime);

    /// "01. Artist - Title.mp3" style heuristics.
    static Tags fromFileName(const std::string& path);

    /// Fills gaps using `ffprobe -show_format -show_streams`.
    static bool readWithFfprobe(const std::string& path,
                                Tags* out,
                                const std::string& ffprobePath = "ffprobe");

    /// Looks for cover.jpg / folder.jpg / front.png next to the track.
    static std::string findSidecarCover(const std::string& path);
    /// Looks for <stem>.lrc / <stem>.txt next to the track.
    static std::string findSidecarLyrics(const std::string& path);
};

} // namespace aurora
