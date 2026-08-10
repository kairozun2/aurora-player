#include "aurora/Decoder.hpp"

#include "aurora/Log.hpp"
#include "aurora/Strings.hpp"

#include <algorithm>
#include <filesystem>
#include <cmath>
#include <cstring>
#include <vector>

namespace aurora {
namespace {

const char* kExtensions[] = {"mp3", "flac", "wav",  "ogg", "oga", "opus", "m4a",
                            "aac", "alac", "aiff", "aif", "wma", "wv",   "mp4",
                            "mka", "webm", "ape",  "mpc", "dsf", "tta"};

/// Sample-rate / channel adapter with linear interpolation.
/// Also implements variable playback speed (0.25x .. 4x) by scaling the step.
class ConverterDecoder final : public IDecoder {
public:
    ConverterDecoder(std::unique_ptr<IDecoder> source, int sampleRate, int channels)
        : source_(std::move(source)), outRate_(sampleRate), outChannels_(channels) {
        inChannels_ = source_->channels() > 0 ? source_->channels() : channels;
        inRate_ = source_->sampleRate() > 0 ? source_->sampleRate() : sampleRate;
        updateStep();
        previous_.assign(static_cast<std::size_t>(inChannels_), 0.0f);
        current_.assign(static_cast<std::size_t>(inChannels_), 0.0f);
    }

    std::size_t read(float* out, std::size_t frames) override {
        if (passthrough()) {
            return source_->read(out, frames);
        }
        std::size_t produced = 0;
        while (produced < frames) {
            while (position_ >= 1.0) {
                if (!advance()) {
                    return produced;
                }
                position_ -= 1.0;
            }
            const float t = static_cast<float>(position_);
            float* dst = out + produced * static_cast<std::size_t>(outChannels_);
            mixChannels(dst, t);
            position_ += step_;
            ++produced;
        }
        return produced;
    }

    bool seek(double seconds) override {
        primed_ = false;
        position_ = 0.0;
        std::fill(previous_.begin(), previous_.end(), 0.0f);
        std::fill(current_.begin(), current_.end(), 0.0f);
        return source_->seek(seconds);
    }

    double duration() const override { return source_->duration(); }
    int sampleRate() const override { return outRate_; }
    int channels() const override { return outChannels_; }
    bool eof() const override { return sourceDone_ && !primed_; }
    std::string name() const override { return source_->name() + "+convert"; }

    void setSpeed(double speed) override {
        if (speed < 0.25) speed = 0.25;
        if (speed > 4.0) speed = 4.0;
        speed_ = speed;
        updateStep();
    }

private:
    bool passthrough() const {
        return inRate_ == outRate_ && inChannels_ == outChannels_ &&
               std::fabs(speed_ - 1.0) < 1e-9;
    }

    void updateStep() {
        step_ = (static_cast<double>(inRate_) / static_cast<double>(outRate_)) * speed_;
        if (step_ <= 0.0) step_ = 1.0;
    }

    bool advance() {
        previous_ = current_;
        const std::size_t got = source_->read(current_.data(), 1);
        if (got == 0) {
            sourceDone_ = true;
            primed_ = false;
            return false;
        }
        primed_ = true;
        return true;
    }

    void mixChannels(float* dst, float t) {
        // Interpolate in the source layout, then map to the output layout.
        if (inChannels_ == outChannels_) {
            for (int c = 0; c < outChannels_; ++c) {
                dst[c] = previous_[static_cast<std::size_t>(c)] * (1.0f - t) +
                         current_[static_cast<std::size_t>(c)] * t;
            }
            return;
        }
        if (inChannels_ == 1) {
            const float value = previous_[0] * (1.0f - t) + current_[0] * t;
            for (int c = 0; c < outChannels_; ++c) dst[c] = value;
            return;
        }
        if (outChannels_ == 1) {
            float sum = 0.0f;
            for (int c = 0; c < inChannels_; ++c) {
                sum += previous_[static_cast<std::size_t>(c)] * (1.0f - t) +
                       current_[static_cast<std::size_t>(c)] * t;
            }
            dst[0] = sum / static_cast<float>(inChannels_);
            return;
        }
        for (int c = 0; c < outChannels_; ++c) {
            const std::size_t srcIndex = static_cast<std::size_t>(c % inChannels_);
            dst[c] = previous_[srcIndex] * (1.0f - t) + current_[srcIndex] * t;
        }
    }

    std::unique_ptr<IDecoder> source_;
    int outRate_;
    int outChannels_;
    int inRate_ = 48000;
    int inChannels_ = 2;
    double position_ = 1.0;  ///< forces priming on first read
    double step_ = 1.0;
    double speed_ = 1.0;
    bool primed_ = false;
    bool sourceDone_ = false;
    std::vector<float> previous_;
    std::vector<float> current_;
};

} // namespace

bool isSupportedAudioFile(const std::string& path) {
    const std::string ext = str::extension(path);
    if (ext.empty()) return false;
    for (const char* candidate : kExtensions) {
        if (ext == candidate) return true;
    }
    return false;
}

std::unique_ptr<IDecoder> makeConverter(std::unique_ptr<IDecoder> source,
                                        int sampleRate,
                                        int channels) {
    if (!source) return nullptr;
    return std::unique_ptr<IDecoder>(
        new ConverterDecoder(std::move(source), sampleRate, channels));
}

std::unique_ptr<IDecoder> makeDecoder(const std::string& uri,
                                      const DecoderOptions& options,
                                      std::string* error) {
    std::string localError;
    const bool isStream = str::isUrl(uri);

    // Local files are validated up front: without this check a typo would be
    // handed to ffmpeg, which fails asynchronously and looks like silence.
    if (!isStream) {
        std::error_code ec;
        if (!std::filesystem::exists(uri, ec) || std::filesystem::is_directory(uri, ec)) {
            if (error) *error = "file not found: " + uri;
            return nullptr;
        }
    }

    if (!isStream && str::extension(uri) == "wav") {
        if (auto wav = makeWavDecoder(uri, &localError)) {
            logDebug("decoder", "native wav backend for " + str::fileName(uri));
            return makeConverter(std::move(wav), options.sampleRate, options.channels);
        }
        logWarn("decoder", "wav backend failed (" + localError + "), falling back to ffmpeg");
    }

    if (auto ff = makeFfmpegDecoder(uri, options, &localError)) {
        // ffmpeg already resamples; the converter only adds speed control.
        return makeConverter(std::move(ff), options.sampleRate, options.channels);
    }

    if (error) {
        *error = localError.empty() ? std::string("no decoder available for ") + uri : localError;
    }
    return nullptr;
}

} // namespace aurora
