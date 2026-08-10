// Aurora Player - waveform peaks for the seek bar and cover-art colour
// extraction for the adaptive (amber-glow) background.
#pragma once

#include "aurora/Decoder.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace aurora {

struct Waveform {
    std::vector<float> peaks;   ///< 0..1, one value per bucket
    std::vector<float> rms;     ///< 0..1, same size as peaks
    double durationSec = 0.0;
    bool valid() const { return !peaks.empty(); }
};

/// Decodes `uri` at a low sample rate in mono and reduces it to `buckets` peaks.
bool computeWaveform(const std::string& uri,
                     int buckets,
                     Waveform* out,
                     const DecoderOptions& options = DecoderOptions(),
                     std::string* error = nullptr);

/// Serialises/loads peaks so the UI does not recompute them on every start.
bool saveWaveform(const std::string& path, const Waveform& waveform);
bool loadWaveform(const std::string& path, Waveform* out);

struct Color {
    unsigned char r = 0;
    unsigned char g = 0;
    unsigned char b = 0;
    std::string hex() const;
    double luminance() const;
};

struct Palette {
    Color dominant;   ///< strongest colour of the artwork
    Color accent;     ///< brighter, saturated companion
    Color muted;      ///< darkened variant, good for backgrounds
    Color text;       ///< black or white, whichever is readable
    bool dark = true; ///< true when the artwork is mostly dark
};

/// Extracts a palette from RGB(A) pixels (stride in bytes, 3 or 4 channels).
Palette paletteFromPixels(const unsigned char* pixels,
                          int width,
                          int height,
                          int channels,
                          int stride = 0);

} // namespace aurora
