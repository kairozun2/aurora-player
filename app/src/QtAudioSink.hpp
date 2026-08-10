// Aurora Player - Qt Multimedia implementation of aurora::ISink.
//
// The engine is device agnostic: it only knows how to fill a float buffer when
// asked. This sink wires that callback to QAudioSink in pull mode, which keeps
// latency low (~40 ms) and needs no extra threads of its own.
#pragma once

#include "aurora/Sink.hpp"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QIODevice>
#include <QMediaDevices>

#include <cstring>
#include <functional>
#include <memory>
#include <vector>

namespace aurora {

/// QIODevice that pulls interleaved float frames straight out of the engine.
///
/// Qt hands us a raw char buffer, so audio is rendered into an aligned staging
/// vector first and then copied. The vector is grown once and reused, so the
/// audio path stays allocation free.
class EnginePullDevice : public QIODevice {
public:
    using Renderer = std::function<void(float*, std::size_t)>;

    explicit EnginePullDevice(QObject* parent = nullptr) : QIODevice(parent) {}

    void setRenderer(Renderer renderer) { renderer_ = std::move(renderer); }
    void setChannels(int channels) { channels_ = channels > 0 ? channels : 2; }

    bool isSequential() const override { return true; }

    /// Qt uses this as a hint for how much it may request.
    qint64 bytesAvailable() const override {
        return 64 * 1024 + QIODevice::bytesAvailable();
    }

protected:
    qint64 readData(char* data, qint64 maxSize) override {
        const qint64 bytesPerFrame = static_cast<qint64>(sizeof(float)) * channels_;
        if (!data || maxSize < bytesPerFrame || !renderer_) return 0;

        const std::size_t frames = static_cast<std::size_t>(maxSize / bytesPerFrame);
        const std::size_t samples = frames * static_cast<std::size_t>(channels_);
        if (staging_.size() < samples) staging_.resize(samples);

        renderer_(staging_.data(), frames);
        std::memcpy(data, staging_.data(), samples * sizeof(float));
        return static_cast<qint64>(samples * sizeof(float));
    }

    qint64 writeData(const char*, qint64) override { return 0; }

private:
    Renderer renderer_;
    std::vector<float> staging_;
    int channels_ = 2;
};

/// Real audio device output. Falls back to the device's preferred format when
/// the requested one is unsupported; the engine then adapts to sampleRate()
/// and channels() automatically.
class QtAudioSink : public ISink {
public:
    ~QtAudioSink() override { stop(); }

    bool start(int sampleRate, int channels, std::string* error) override {
        const QAudioDevice output = QMediaDevices::defaultAudioOutput();
        if (output.isNull()) {
            if (error) *error = "no audio output device available";
            return false;
        }

        QAudioFormat wanted;
        wanted.setSampleRate(sampleRate > 0 ? sampleRate : 48000);
        wanted.setChannelCount(channels > 0 ? channels : 2);
        wanted.setSampleFormat(QAudioFormat::Float);

        if (!output.isFormatSupported(wanted)) {
            QAudioFormat fallback = output.preferredFormat();
            fallback.setSampleFormat(QAudioFormat::Float);
            if (!output.isFormatSupported(fallback)) fallback = output.preferredFormat();
            wanted = fallback;
        }
        format_ = wanted;

        device_ = std::make_unique<EnginePullDevice>();
        device_->setChannels(format_.channelCount());
        device_->setRenderer([this](float* out, std::size_t frames) { renderInto(out, frames); });
        if (!device_->open(QIODevice::ReadOnly)) {
            if (error) *error = "cannot open the audio stream";
            device_.reset();
            return false;
        }

        sink_ = std::make_unique<QAudioSink>(output, format_);
        // ~40 ms of buffering: responsive controls without dropouts.
        sink_->setBufferSize(static_cast<int>(format_.bytesForDuration(40 * 1000)));
        sink_->start(device_.get());

        if (sink_->error() != QAudio::NoError) {
            if (error) *error = "the audio device refused to start";
            stop();
            return false;
        }
        running_ = true;
        return true;
    }

    void stop() override {
        if (sink_) {
            sink_->stop();
            sink_.reset();
        }
        if (device_) {
            device_->close();
            device_.reset();
        }
        running_ = false;
    }

    bool isRunning() const override { return running_; }
    int sampleRate() const override { return format_.sampleRate() > 0 ? format_.sampleRate() : 48000; }
    int channels() const override { return format_.channelCount() > 0 ? format_.channelCount() : 2; }
    std::string name() const override { return "qt-audio"; }

    /// Human readable device name for the settings screen.
    static QString deviceDescription() {
        const QAudioDevice output = QMediaDevices::defaultAudioOutput();
        return output.isNull() ? QStringLiteral("-") : output.description();
    }

private:
    QAudioFormat format_;
    std::unique_ptr<QAudioSink> sink_;
    std::unique_ptr<EnginePullDevice> device_;
    bool running_ = false;
};

} // namespace aurora
