#include "aurora/Analysis.hpp"

#include "aurora/Json.hpp"
#include "aurora/Log.hpp"
#include "aurora/Strings.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <vector>

namespace aurora {
namespace {

constexpr int kAnalysisRate = 8000;  ///< enough for a waveform, ~6x faster than 48k

} // namespace

std::string Color::hex() const {
    char buffer[8];
    std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X", r, g, b);
    return std::string(buffer);
}

double Color::luminance() const {
    // Rec. 709 relative luminance
    return (0.2126 * r + 0.7152 * g + 0.0722 * b) / 255.0;
}

bool computeWaveform(const std::string& uri, int buckets, Waveform* out,
                     const DecoderOptions& options, std::string* error) {
    if (!out || buckets <= 0) return false;
    DecoderOptions analysis = options;
    analysis.sampleRate = kAnalysisRate;
    analysis.channels = 1;

    std::string openError;
    auto decoder = makeDecoder(uri, analysis, &openError);
    if (!decoder) {
        if (error) *error = openError;
        return false;
    }

    const double duration = decoder->duration();
    std::vector<float> samples;
    samples.reserve(static_cast<std::size_t>(kAnalysisRate) * 60);
    std::vector<float> chunk(4096);
    std::size_t got = 0;
    // Cap the analysis at 30 minutes so a broken stream cannot exhaust memory.
    const std::size_t maxSamples = static_cast<std::size_t>(kAnalysisRate) * 60 * 30;
    while ((got = decoder->read(chunk.data(), chunk.size())) > 0) {
        samples.insert(samples.end(), chunk.begin(), chunk.begin() + static_cast<long>(got));
        if (samples.size() >= maxSamples) break;
    }
    if (samples.empty()) {
        if (error) *error = "no audio decoded";
        return false;
    }

    out->peaks.assign(static_cast<std::size_t>(buckets), 0.0f);
    out->rms.assign(static_cast<std::size_t>(buckets), 0.0f);
    out->durationSec = duration > 0.0 ? duration
                                     : static_cast<double>(samples.size()) / kAnalysisRate;

    const double perBucket = static_cast<double>(samples.size()) / static_cast<double>(buckets);
    for (int b = 0; b < buckets; ++b) {
        const std::size_t begin = static_cast<std::size_t>(b * perBucket);
        std::size_t end = static_cast<std::size_t>((b + 1) * perBucket);
        if (end > samples.size()) end = samples.size();
        float peak = 0.0f;
        double sum = 0.0;
        for (std::size_t i = begin; i < end; ++i) {
            const float value = std::fabs(samples[i]);
            if (value > peak) peak = value;
            sum += static_cast<double>(samples[i]) * samples[i];
        }
        const std::size_t count = end > begin ? end - begin : 1;
        out->peaks[static_cast<std::size_t>(b)] = std::min(1.0f, peak);
        out->rms[static_cast<std::size_t>(b)] =
            std::min(1.0f, static_cast<float>(std::sqrt(sum / static_cast<double>(count))));
    }
    return true;
}

bool saveWaveform(const std::string& path, const Waveform& waveform) {
    Json root = Json::object();
    root.set("duration", waveform.durationSec);
    Json peaks = Json::array();
    for (const float value : waveform.peaks) peaks.push(Json(static_cast<double>(value)));
    Json rms = Json::array();
    for (const float value : waveform.rms) rms.push(Json(static_cast<double>(value)));
    root.set("peaks", peaks);
    root.set("rms", rms);
    return root.saveFile(path, 0);
}

bool loadWaveform(const std::string& path, Waveform* out) {
    if (!out) return false;
    std::string error;
    const Json root = Json::parseFile(path, &error);
    if (!error.empty()) return false;
    out->durationSec = root["duration"].asDouble();
    const Json& peaks = root["peaks"];
    const Json& rms = root["rms"];
    out->peaks.clear();
    out->rms.clear();
    for (std::size_t i = 0; i < peaks.size(); ++i) {
        out->peaks.push_back(static_cast<float>(peaks.at(i).asDouble()));
    }
    for (std::size_t i = 0; i < rms.size(); ++i) {
        out->rms.push_back(static_cast<float>(rms.at(i).asDouble()));
    }
    return !out->peaks.empty();
}

Palette paletteFromPixels(const unsigned char* pixels, int width, int height, int channels,
                          int stride) {
    Palette palette;
    if (!pixels || width <= 0 || height <= 0 || channels < 3) return palette;
    if (stride <= 0) stride = width * channels;

    // Coarse 4-bit-per-channel histogram, weighted by saturation so that grey
    // backgrounds do not win over the actual artwork colour.
    std::map<int, double> histogram;
    std::map<int, std::vector<double>> sums;
    const int step = std::max(1, (width * height) / 40000);
    long long totalPixels = 0;
    double luminanceSum = 0.0;

    for (int y = 0; y < height; ++y) {
        const unsigned char* row = pixels + static_cast<std::size_t>(y) * static_cast<std::size_t>(stride);
        for (int x = 0; x < width; x += step) {
            const unsigned char* p = row + static_cast<std::size_t>(x) * static_cast<std::size_t>(channels);
            if (channels == 4 && p[3] < 32) continue;
            const double r = p[0];
            const double g = p[1];
            const double b = p[2];
            const double maxValue = std::max(r, std::max(g, b));
            const double minValue = std::min(r, std::min(g, b));
            const double saturation = maxValue <= 0.0 ? 0.0 : (maxValue - minValue) / maxValue;
            const double weight = 0.15 + saturation * 1.85;
            const int key = ((static_cast<int>(r) >> 4) << 8) | ((static_cast<int>(g) >> 4) << 4) |
                            (static_cast<int>(b) >> 4);
            histogram[key] += weight;
            std::vector<double>& accumulator = sums[key];
            if (accumulator.empty()) accumulator.assign(4, 0.0);
            accumulator[0] += r * weight;
            accumulator[1] += g * weight;
            accumulator[2] += b * weight;
            accumulator[3] += weight;
            luminanceSum += (0.2126 * r + 0.7152 * g + 0.0722 * b) / 255.0;
            ++totalPixels;
        }
    }
    if (histogram.empty() || totalPixels == 0) return palette;

    int bestKey = histogram.begin()->first;
    double bestWeight = 0.0;
    for (const auto& entry : histogram) {
        if (entry.second > bestWeight) {
            bestWeight = entry.second;
            bestKey = entry.first;
        }
    }
    const std::vector<double>& best = sums[bestKey];
    auto clamp255 = [](double value) {
        if (value < 0.0) value = 0.0;
        if (value > 255.0) value = 255.0;
        return static_cast<unsigned char>(value + 0.5);
    };
    palette.dominant.r = clamp255(best[0] / best[3]);
    palette.dominant.g = clamp255(best[1] / best[3]);
    palette.dominant.b = clamp255(best[2] / best[3]);

    // Accent: brighten and saturate the dominant colour.
    palette.accent.r = clamp255(palette.dominant.r * 1.35 + 18);
    palette.accent.g = clamp255(palette.dominant.g * 1.28 + 14);
    palette.accent.b = clamp255(palette.dominant.b * 1.18 + 10);

    // Muted: darkened variant for glass backgrounds.
    palette.muted.r = clamp255(palette.dominant.r * 0.38);
    palette.muted.g = clamp255(palette.dominant.g * 0.36);
    palette.muted.b = clamp255(palette.dominant.b * 0.40);

    palette.dark = (luminanceSum / static_cast<double>(totalPixels)) < 0.5;
    const bool lightBackground = palette.dominant.luminance() > 0.6;
    palette.text.r = palette.text.g = palette.text.b = lightBackground ? 20 : 255;
    return palette;
}

} // namespace aurora
