#include "aurora/Equalizer.hpp"

#include "aurora/Strings.hpp"

#include <cmath>

namespace aurora {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr float kMaxGainDb = 12.0f;
constexpr double kQ = 1.1;  // ~1.3 octave bands, smooth overlap

const int kFrequencies[Equalizer::kBands] = {32, 64, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};

struct Preset {
    const char* name;
    float gains[Equalizer::kBands];
};

const Preset kPresets[] = {
    {"flat",       {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"bass",       {7.5f, 6.0f, 4.0f, 1.5f, 0, -1.0f, -1.0f, 0, 1.0f, 2.0f}},
    {"treble",     {-2.0f, -1.5f, -1.0f, 0, 0, 1.0f, 2.5f, 4.5f, 6.0f, 6.5f}},
    {"vocal",      {-3.0f, -2.0f, 0, 2.0f, 4.0f, 4.5f, 3.0f, 1.5f, 0, -1.0f}},
    {"rock",       {5.0f, 3.5f, 1.0f, -1.0f, -1.5f, 0.5f, 2.5f, 4.0f, 4.5f, 4.0f}},
    {"pop",        {-1.0f, 1.0f, 3.0f, 4.0f, 3.0f, 1.0f, -0.5f, -1.0f, 0.5f, 2.0f}},
    {"jazz",       {3.5f, 2.5f, 1.0f, 1.5f, -1.0f, -1.0f, 0, 1.5f, 3.0f, 3.5f}},
    {"classical",  {3.0f, 2.0f, 0.5f, 0, 0, 0, -1.0f, -1.5f, 1.5f, 3.0f}},
    {"electronic", {6.0f, 5.0f, 2.0f, 0, -1.5f, 1.5f, 1.0f, 2.0f, 4.5f, 5.5f}},
    {"podcast",    {-6.0f, -4.0f, -1.0f, 3.0f, 5.0f, 5.0f, 3.5f, 1.0f, -1.5f, -3.0f}},
    {"night",      {-4.0f, -3.0f, -1.0f, 1.5f, 3.0f, 3.5f, 2.0f, 0.5f, -2.0f, -4.0f}},
};

float clampDb(float db) {
    if (db > kMaxGainDb) return kMaxGainDb;
    if (db < -kMaxGainDb) return -kMaxGainDb;
    return db;
}

} // namespace

Equalizer::Equalizer() {
    for (int i = 0; i < kBands; ++i) gainsDb_[static_cast<std::size_t>(i)].store(0.0f);
    recomputeCoefficients();
}

void Equalizer::setSampleRate(int sampleRate) {
    if (sampleRate <= 0 || sampleRate == sampleRate_) return;
    sampleRate_ = sampleRate;
    dirty_.store(true);
    resetState();
}

void Equalizer::setBandGain(int band, float db) {
    if (band < 0 || band >= kBands) return;
    gainsDb_[static_cast<std::size_t>(band)].store(clampDb(db));
    preset_ = "custom";
    dirty_.store(true);
}

float Equalizer::bandGain(int band) const {
    if (band < 0 || band >= kBands) return 0.0f;
    return gainsDb_[static_cast<std::size_t>(band)].load();
}

std::vector<float> Equalizer::gains() const {
    std::vector<float> out;
    out.reserve(kBands);
    for (int i = 0; i < kBands; ++i) out.push_back(gainsDb_[static_cast<std::size_t>(i)].load());
    return out;
}

void Equalizer::setGains(const std::vector<float>& db) {
    for (int i = 0; i < kBands; ++i) {
        const float value = static_cast<std::size_t>(i) < db.size() ? db[static_cast<std::size_t>(i)] : 0.0f;
        gainsDb_[static_cast<std::size_t>(i)].store(clampDb(value));
    }
    preset_ = "custom";
    dirty_.store(true);
}

void Equalizer::setPreampDb(float db) { preampDb_.store(clampDb(db)); }

std::vector<int> Equalizer::bandFrequencies() {
    return std::vector<int>(kFrequencies, kFrequencies + kBands);
}

std::vector<std::string> Equalizer::presetNames() {
    std::vector<std::string> names;
    for (const Preset& preset : kPresets) names.push_back(preset.name);
    return names;
}

std::vector<float> Equalizer::presetGains(const std::string& name) {
    const std::string wanted = str::toLower(name);
    for (const Preset& preset : kPresets) {
        if (wanted == preset.name) {
            return std::vector<float>(preset.gains, preset.gains + kBands);
        }
    }
    return std::vector<float>(static_cast<std::size_t>(kBands), 0.0f);
}

bool Equalizer::applyPreset(const std::string& name) {
    const std::string wanted = str::toLower(name);
    for (const Preset& preset : kPresets) {
        if (wanted == preset.name) {
            for (int i = 0; i < kBands; ++i) {
                gainsDb_[static_cast<std::size_t>(i)].store(clampDb(preset.gains[i]));
            }
            preset_ = wanted;
            dirty_.store(true);
            return true;
        }
    }
    return false;
}

void Equalizer::resetState() {
    for (auto& channel : states_) {
        for (auto& state : channel) state = State();
    }
}

void Equalizer::recomputeCoefficients() {
    for (int i = 0; i < kBands; ++i) {
        const double gainDb = static_cast<double>(gainsDb_[static_cast<std::size_t>(i)].load());
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * kPi * static_cast<double>(kFrequencies[i]) / static_cast<double>(sampleRate_);
        const double alpha = std::sin(w0) / (2.0 * kQ);
        const double cosw0 = std::cos(w0);

        const double b0 = 1.0 + alpha * A;
        const double b1 = -2.0 * cosw0;
        const double b2 = 1.0 - alpha * A;
        const double a0 = 1.0 + alpha / A;
        const double a1 = -2.0 * cosw0;
        const double a2 = 1.0 - alpha / A;

        Biquad& c = coefficients_[static_cast<std::size_t>(i)];
        c.b0 = b0 / a0;
        c.b1 = b1 / a0;
        c.b2 = b2 / a0;
        c.a1 = a1 / a0;
        c.a2 = a2 / a0;
    }
}

void Equalizer::process(float* samples, std::size_t frames, int channels) {
    if (!samples || frames == 0 || channels <= 0) return;
    const float preamp = std::pow(10.0f, preampDb_.load() / 20.0f);
    const bool active = enabled_.load();
    if (!active) {
        if (std::fabs(preamp - 1.0f) > 1e-6f) {
            const std::size_t total = frames * static_cast<std::size_t>(channels);
            for (std::size_t i = 0; i < total; ++i) samples[i] *= preamp;
        }
        return;
    }
    if (dirty_.exchange(false)) recomputeCoefficients();

    const int usedChannels = channels > kMaxChannels ? kMaxChannels : channels;
    for (std::size_t f = 0; f < frames; ++f) {
        for (int ch = 0; ch < usedChannels; ++ch) {
            double sample = static_cast<double>(samples[f * static_cast<std::size_t>(channels) +
                                                        static_cast<std::size_t>(ch)]) * preamp;
            for (int b = 0; b < kBands; ++b) {
                const Biquad& c = coefficients_[static_cast<std::size_t>(b)];
                State& s = states_[static_cast<std::size_t>(ch)][static_cast<std::size_t>(b)];
                const double out = c.b0 * sample + c.b1 * s.x1 + c.b2 * s.x2 -
                                   c.a1 * s.y1 - c.a2 * s.y2;
                s.x2 = s.x1;
                s.x1 = sample;
                s.y2 = s.y1;
                s.y1 = out;
                sample = out;
            }
            // Guard against denormals and NaN sneaking into the device buffer.
            if (!(sample > -1e6 && sample < 1e6)) sample = 0.0;
            samples[f * static_cast<std::size_t>(channels) + static_cast<std::size_t>(ch)] =
                static_cast<float>(sample);
        }
    }
}

} // namespace aurora
