#include "aurora/Sink.hpp"

#include "aurora/Log.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

namespace aurora {
namespace {

constexpr std::size_t kBlockFrames = 512;

class NullSink final : public ISink {
public:
    explicit NullSink(double speedFactor) : speed_(speedFactor <= 0 ? 1.0 : speedFactor) {}
    ~NullSink() override { stop(); }

    bool start(int sampleRate, int channels, std::string* error) override {
        (void)error;
        stop();
        rate_ = sampleRate;
        channels_ = channels;
        running_.store(true);
        thread_ = std::thread([this] { loop(); });
        return true;
    }

    void stop() override {
        running_.store(false);
        if (thread_.joinable()) thread_.join();
    }

    bool isRunning() const override { return running_.load(); }
    int sampleRate() const override { return rate_; }
    int channels() const override { return channels_; }
    std::string name() const override { return "null"; }

private:
    void loop() {
        std::vector<float> block(kBlockFrames * static_cast<std::size_t>(channels_));
        const double blockSeconds = static_cast<double>(kBlockFrames) / rate_ / speed_;
        auto next = std::chrono::steady_clock::now();
        while (running_.load()) {
            renderInto(block.data(), kBlockFrames);
            next += std::chrono::microseconds(static_cast<long long>(blockSeconds * 1e6));
            std::this_thread::sleep_until(next);
        }
    }

    double speed_;
    int rate_ = 48000;
    int channels_ = 2;
    std::atomic<bool> running_{false};
    std::thread thread_;
};

class WavFileSink final : public ISink {
public:
    WavFileSink(std::string path, double maxSeconds)
        : path_(std::move(path)), maxSeconds_(maxSeconds) {}
    ~WavFileSink() override { stop(); }

    bool start(int sampleRate, int channels, std::string* error) override {
        stop();
        rate_ = sampleRate;
        channels_ = channels;
        file_ = std::fopen(path_.c_str(), "wb");
        if (!file_) {
            if (error) *error = "cannot write " + path_;
            return false;
        }
        writeHeader(0);
        framesWritten_ = 0;
        running_.store(true);
        thread_ = std::thread([this] { loop(); });
        return true;
    }

    void stop() override {
        running_.store(false);
        if (thread_.joinable()) thread_.join();
        if (file_) {
            std::fseek(file_, 0, SEEK_SET);
            writeHeader(framesWritten_);
            std::fclose(file_);
            file_ = nullptr;
            logInfo("sink", "rendered " + std::to_string(framesWritten_) + " frames to " + path_);
        }
    }

    bool isRunning() const override { return running_.load(); }
    int sampleRate() const override { return rate_; }
    int channels() const override { return channels_; }
    std::string name() const override { return "wavfile"; }
    bool offline() const override { return true; }

private:
    void loop() {
        const std::size_t limit =
            maxSeconds_ > 0 ? static_cast<std::size_t>(maxSeconds_ * rate_) : 0;
        std::vector<float> block(kBlockFrames * static_cast<std::size_t>(channels_));
        std::vector<std::int16_t> pcm(block.size());
        while (running_.load()) {
            renderInto(block.data(), kBlockFrames);
            for (std::size_t i = 0; i < block.size(); ++i) {
                float v = block[i];
                if (v > 1.0f) v = 1.0f;
                if (v < -1.0f) v = -1.0f;
                pcm[i] = static_cast<std::int16_t>(v * 32767.0f);
            }
            std::fwrite(pcm.data(), sizeof(std::int16_t), pcm.size(), file_);
            framesWritten_ += kBlockFrames;
            if (limit && framesWritten_ >= limit) break;
        }
        running_.store(false);
    }

    void writeU32(std::uint32_t value) { std::fwrite(&value, 4, 1, file_); }
    void writeU16(std::uint16_t value) { std::fwrite(&value, 2, 1, file_); }

    void writeHeader(std::size_t frames) {
        const std::uint32_t dataBytes =
            static_cast<std::uint32_t>(frames * static_cast<std::size_t>(channels_) * 2);
        std::fwrite("RIFF", 1, 4, file_);
        writeU32(36 + dataBytes);
        std::fwrite("WAVEfmt ", 1, 8, file_);
        writeU32(16);
        writeU16(1);
        writeU16(static_cast<std::uint16_t>(channels_));
        writeU32(static_cast<std::uint32_t>(rate_));
        writeU32(static_cast<std::uint32_t>(rate_ * channels_ * 2));
        writeU16(static_cast<std::uint16_t>(channels_ * 2));
        writeU16(16);
        std::fwrite("data", 1, 4, file_);
        writeU32(dataBytes);
    }

    std::string path_;
    double maxSeconds_;
    std::FILE* file_ = nullptr;
    int rate_ = 48000;
    int channels_ = 2;
    std::size_t framesWritten_ = 0;
    std::atomic<bool> running_{false};
    std::thread thread_;
};

} // namespace

std::unique_ptr<ISink> makeNullSink(double speedFactor) {
    return std::unique_ptr<ISink>(new NullSink(speedFactor));
}

std::unique_ptr<ISink> makeWavFileSink(const std::string& path, double maxSeconds) {
    return std::unique_ptr<ISink>(new WavFileSink(path, maxSeconds));
}

} // namespace aurora
