// Aurora Player - 10-band parametric equalizer (RBJ peaking biquads).
// Gains are stored atomically so the UI can move sliders while audio is
// rendering, without locking the audio thread.
#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <string>
#include <vector>

namespace aurora {

class Equalizer {
public:
    static constexpr int kBands = 10;
    static constexpr int kMaxChannels = 8;

    Equalizer();

    void setSampleRate(int sampleRate);
    int sampleRate() const { return sampleRate_; }

    void setEnabled(bool enabled) { enabled_.store(enabled); }
    bool enabled() const { return enabled_.load(); }

    /// Band gain in dB, clamped to [-12, +12].
    void setBandGain(int band, float db);
    float bandGain(int band) const;
    std::vector<float> gains() const;
    void setGains(const std::vector<float>& db);

    /// Global pre-amplification in dB, clamped to [-12, +12].
    void setPreampDb(float db);
    float preampDb() const { return preampDb_.load(); }

    static std::vector<int> bandFrequencies();
    static std::vector<std::string> presetNames();
    static std::vector<float> presetGains(const std::string& name);
    bool applyPreset(const std::string& name);
    const std::string& currentPreset() const { return preset_; }

    /// Processes interleaved audio in place.
    void process(float* samples, std::size_t frames, int channels);
    void resetState();

private:
    struct Biquad {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    };
    struct State {
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    };

    void recomputeCoefficients();

    int sampleRate_ = 48000;
    std::atomic<bool> enabled_{false};
    std::atomic<bool> dirty_{true};
    std::atomic<float> preampDb_{0.0f};
    std::array<std::atomic<float>, kBands> gainsDb_;
    std::array<Biquad, kBands> coefficients_;
    std::array<std::array<State, kBands>, kMaxChannels> states_;
    std::string preset_ = "flat";
};

} // namespace aurora
