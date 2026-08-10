// Aurora Player - playback engine.
//
// Design notes
// ------------
// * One decode thread fills a lock-free ring buffer; the sink pulls from it.
//   The audio callback never allocates, never locks and never blocks on I/O.
// * Two streams are kept side by side, so the next track can be decoded while
//   the current one still plays: that gives gapless playback and crossfade.
// * Volume changes are ramped per sample to avoid clicks.
#pragma once

#include "aurora/Decoder.hpp"
#include "aurora/Equalizer.hpp"
#include "aurora/RingBuffer.hpp"
#include "aurora/Sink.hpp"
#include "aurora/Types.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace aurora {

class AudioEngine {
public:
    using StateCallback = std::function<void(PlaybackState)>;
    using PositionCallback = std::function<void(double positionSec, double durationSec)>;
    using FinishedCallback = std::function<void()>;
    using AdvancedCallback = std::function<void(const std::string& uri)>;
    using ErrorCallback = std::function<void(const std::string& message)>;
    /// Returns the URI that should play after the current one, or "" if none.
    using NextUriProvider = std::function<std::string()>;

    AudioEngine();
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    /// Takes ownership of the sink and starts the decode thread.
    bool start(std::unique_ptr<ISink> sink, std::string* error);
    void shutdown();
    bool running() const { return running_.load(); }

    bool load(const std::string& uri, bool autoPlay, std::string* error);
    void play();
    void pause();
    void togglePlayPause();
    void stop();
    bool seek(double seconds);
    bool seekRelative(double deltaSeconds);

    void setVolume(float volume);
    float volume() const { return volume_.load(); }
    void setMuted(bool muted);
    bool muted() const { return muted_.load(); }
    void setSpeed(double speed);
    double speed() const { return speed_.load(); }

    /// 0 disables crossfade (gapless still applies when enabled).
    void setCrossfadeSeconds(double seconds);
    double crossfadeSeconds() const { return crossfade_.load(); }
    void setGapless(bool enabled) { gapless_.store(enabled); }
    bool gapless() const { return gapless_.load(); }

    void setNextUriProvider(NextUriProvider provider);
    void setDecoderOptions(const DecoderOptions& options);

    PlaybackState state() const { return state_.load(); }
    double position() const;
    double duration() const;
    std::string currentUri() const;
    PlayerSnapshot snapshot() const;
    EngineMetrics metrics() const;
    void levels(float* left, float* right) const;

    Equalizer& equalizer() { return equalizer_; }
    const Equalizer& equalizer() const { return equalizer_; }

    void onStateChanged(StateCallback callback);
    void onPositionChanged(PositionCallback callback);
    void onFinished(FinishedCallback callback);
    void onTrackAdvanced(AdvancedCallback callback);
    void onError(ErrorCallback callback);

private:
    struct Stream {
        std::unique_ptr<IDecoder> decoder;   ///< decode thread only
        RingBuffer ring;                      ///< single producer / single consumer
        std::string uri;
        std::atomic<bool> eof{false};
        std::atomic<std::uint64_t> consumed{0};
        std::atomic<double> offset{0.0};
        std::atomic<double> duration{0.0};

        void resetCounters() {
            eof.store(false);
            consumed.store(0);
            offset.store(0.0);
            ring.clear();
        }
    };

    void render(float* out, std::size_t frames);
    void decodeLoop();
    void fillStream(Stream& stream, std::size_t chunkFrames);
    void setState(PlaybackState state);
    double positionOf(const Stream& stream) const;
    Stream& active() { return streams_[activeIndex_.load()]; }
    Stream& standby() { return streams_[1 - activeIndex_.load()]; }

    std::unique_ptr<ISink> sink_;
    Stream streams_[2];
    std::atomic<int> activeIndex_{0};

    std::thread decodeThread_;
    mutable std::mutex mutex_;          ///< guards decoder ownership + callbacks
    std::atomic<bool> running_{false};
    std::atomic<PlaybackState> state_{PlaybackState::Stopped};

    std::atomic<float> volume_{1.0f};
    std::atomic<bool> muted_{false};
    std::atomic<double> speed_{1.0};
    std::atomic<double> crossfade_{0.0};
    std::atomic<bool> gapless_{true};
    std::atomic<bool> crossfadeActive_{false};
    std::atomic<double> pendingSeek_{-1.0};
    std::atomic<bool> finishedPending_{false};
    std::atomic<bool> advancedPending_{false};
    /// True while a stream is being (re)filled, so silence is not an underrun.
    std::atomic<bool> priming_{false};
    /// True when rendering into a file instead of a device.
    std::atomic<bool> offlineSink_{false};

    // hot-path state, touched by the render callback only
    float gain_ = 0.0f;
    float gainTarget_ = 0.0f;
    std::atomic<float> levelLeft_{0.0f};
    std::atomic<float> levelRight_{0.0f};

    std::atomic<std::uint64_t> framesRendered_{0};
    std::atomic<std::uint64_t> underruns_{0};
    std::atomic<std::uint64_t> decodedFrames_{0};
    std::atomic<double> renderCpuMs_{0.0};

    Equalizer equalizer_;
    DecoderOptions decoderOptions_;
    NextUriProvider nextUriProvider_;
    StateCallback stateCallback_;
    PositionCallback positionCallback_;
    FinishedCallback finishedCallback_;
    AdvancedCallback advancedCallback_;
    ErrorCallback errorCallback_;
    std::string advancedUri_;
    double lastReportedPosition_ = -1.0;
};

} // namespace aurora
