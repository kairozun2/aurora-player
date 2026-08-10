#include "aurora/AudioEngine.hpp"

#include "aurora/Log.hpp"
#include "aurora/Strings.hpp"

#include <algorithm>
#include <chrono>
#include <thread>
#include <cmath>
#include <vector>

namespace aurora {
namespace {

constexpr std::size_t kRingFrames = 1u << 15;   // ~0.68 s at 48 kHz
constexpr std::size_t kDecodeChunk = 2048;      // frames per decode pass
constexpr float kGainSmoothing = 0.002f;        // ~12 ms fade at 48 kHz

/// Gentle limiter: transparent below -1 dBFS, soft knee above it. Keeps the
/// output stable when the equalizer boosts several bands at once.
inline float softClip(float x) {
    const float threshold = 0.89f;
    if (x > threshold) {
        return threshold + (1.0f - threshold) * std::tanh((x - threshold) / (1.0f - threshold));
    }
    if (x < -threshold) {
        return -threshold + (1.0f - threshold) * std::tanh((x + threshold) / (1.0f - threshold));
    }
    return x;
}

} // namespace

AudioEngine::AudioEngine() {
    streams_[0].ring.reset(kRingFrames * 2);
    streams_[1].ring.reset(kRingFrames * 2);
}

AudioEngine::~AudioEngine() { shutdown(); }

bool AudioEngine::start(std::unique_ptr<ISink> sink, std::string* error) {
    shutdown();
    if (!sink) {
        if (error) *error = "no audio sink provided";
        return false;
    }
    sink_ = std::move(sink);
    sink_->setRenderCallback([this](float* out, std::size_t frames) { render(out, frames); });
    if (!sink_->start(decoderOptions_.sampleRate, decoderOptions_.channels, error)) {
        sink_.reset();
        return false;
    }
    decoderOptions_.sampleRate = sink_->sampleRate();
    decoderOptions_.channels = sink_->channels();
    offlineSink_.store(sink_->offline());
    equalizer_.setSampleRate(sink_->sampleRate());
    const std::size_t ringSamples = kRingFrames * static_cast<std::size_t>(decoderOptions_.channels);
    streams_[0].ring.reset(ringSamples);
    streams_[1].ring.reset(ringSamples);
    running_.store(true);
    decodeThread_ = std::thread([this] { decodeLoop(); });
    logInfo("engine", std::string("started with sink '") + sink_->name() + "' at " +
                          std::to_string(sink_->sampleRate()) + " Hz / " +
                          std::to_string(sink_->channels()) + " ch");
    return true;
}

void AudioEngine::shutdown() {
    if (!running_.exchange(false)) {
        return;
    }
    if (decodeThread_.joinable()) decodeThread_.join();
    if (sink_) {
        sink_->stop();
        sink_.reset();
    }
    std::lock_guard<std::mutex> lock(mutex_);
    streams_[0].decoder.reset();
    streams_[1].decoder.reset();
    state_.store(PlaybackState::Stopped);
}

void AudioEngine::setDecoderOptions(const DecoderOptions& options) {
    std::lock_guard<std::mutex> lock(mutex_);
    const int rate = decoderOptions_.sampleRate;
    const int channels = decoderOptions_.channels;
    decoderOptions_ = options;
    if (sink_) {  // the running device dictates the format
        decoderOptions_.sampleRate = rate;
        decoderOptions_.channels = channels;
    }
}

bool AudioEngine::load(const std::string& uri, bool autoPlay, std::string* error) {
    if (uri.empty()) {
        if (error) *error = "empty uri";
        return false;
    }
    std::string openError;
    DecoderOptions options;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        options = decoderOptions_;
    }
    auto decoder = makeDecoder(uri, options, &openError);
    if (!decoder) {
        setState(PlaybackState::Error);
        ErrorCallback callback;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            callback = errorCallback_;
        }
        if (callback) callback(openError);
        if (error) *error = openError;
        logError("engine", "load failed: " + openError);
        return false;
    }
    decoder->setSpeed(speed_.load());

    {
        std::lock_guard<std::mutex> lock(mutex_);
        activeIndex_.store(0);
        crossfadeActive_.store(false);
        pendingSeek_.store(-1.0);
        finishedPending_.store(false);
        advancedPending_.store(false);
        for (Stream& stream : streams_) {
            stream.decoder.reset();
            stream.uri.clear();
            stream.resetCounters();
            stream.duration.store(0.0);
        }
        Stream& stream = streams_[0];
        stream.duration.store(decoder->duration());
        stream.uri = uri;
        stream.decoder = std::move(decoder);
        equalizer_.resetState();
        // Pre-roll audio before the first render call: playback starts from a
        // full buffer instead of racing the decode thread.
        priming_.store(true);
        fillStream(stream, kDecodeChunk);
        priming_.store(false);
        lastReportedPosition_ = -1.0;
    }
    logInfo("engine", "loaded " + str::fileName(uri));
    setState(autoPlay ? PlaybackState::Playing : PlaybackState::Paused);
    gainTarget_ = autoPlay ? 1.0f : 0.0f;
    return true;
}

void AudioEngine::play() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!streams_[activeIndex_.load()].decoder) return;
    }
    gainTarget_ = 1.0f;
    setState(PlaybackState::Playing);
}

void AudioEngine::pause() {
    if (state_.load() != PlaybackState::Playing) return;
    gainTarget_ = 0.0f;
    setState(PlaybackState::Paused);
}

void AudioEngine::togglePlayPause() {
    if (state_.load() == PlaybackState::Playing) {
        pause();
    } else {
        play();
    }
}

void AudioEngine::stop() {
    gainTarget_ = 0.0f;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (Stream& stream : streams_) {
            stream.decoder.reset();
            stream.uri.clear();
            stream.resetCounters();
        }
        activeIndex_.store(0);
        crossfadeActive_.store(false);
    }
    setState(PlaybackState::Stopped);
}

bool AudioEngine::seek(double seconds) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!streams_[activeIndex_.load()].decoder) return false;
    }
    if (seconds < 0) seconds = 0;
    const double total = duration();
    if (total > 0 && seconds > total) seconds = total;
    pendingSeek_.store(seconds);
    return true;
}

bool AudioEngine::seekRelative(double deltaSeconds) { return seek(position() + deltaSeconds); }

void AudioEngine::setVolume(float volume) {
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.5f) volume = 1.5f;
    volume_.store(volume);
}

void AudioEngine::setMuted(bool muted) { muted_.store(muted); }

void AudioEngine::setSpeed(double speed) {
    if (speed < 0.25) speed = 0.25;
    if (speed > 4.0) speed = 4.0;
    speed_.store(speed);
    std::lock_guard<std::mutex> lock(mutex_);
    for (Stream& stream : streams_) {
        if (stream.decoder) stream.decoder->setSpeed(speed);
    }
}

void AudioEngine::setCrossfadeSeconds(double seconds) {
    if (seconds < 0) seconds = 0;
    if (seconds > 12) seconds = 12;
    crossfade_.store(seconds);
}

void AudioEngine::setNextUriProvider(NextUriProvider provider) {
    std::lock_guard<std::mutex> lock(mutex_);
    nextUriProvider_ = std::move(provider);
}

void AudioEngine::onStateChanged(StateCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    stateCallback_ = std::move(callback);
}
void AudioEngine::onPositionChanged(PositionCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    positionCallback_ = std::move(callback);
}
void AudioEngine::onFinished(FinishedCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    finishedCallback_ = std::move(callback);
}
void AudioEngine::onTrackAdvanced(AdvancedCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    advancedCallback_ = std::move(callback);
}
void AudioEngine::onError(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    errorCallback_ = std::move(callback);
}

double AudioEngine::positionOf(const Stream& stream) const {
    const int rate = decoderOptions_.sampleRate > 0 ? decoderOptions_.sampleRate : 48000;
    return stream.offset.load() +
           static_cast<double>(stream.consumed.load()) / static_cast<double>(rate);
}

double AudioEngine::position() const {
    return positionOf(streams_[activeIndex_.load()]);
}

double AudioEngine::duration() const {
    return streams_[activeIndex_.load()].duration.load();
}

std::string AudioEngine::currentUri() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return streams_[activeIndex_.load()].uri;
}

PlayerSnapshot AudioEngine::snapshot() const {
    PlayerSnapshot snapshot;
    snapshot.state = state_.load();
    snapshot.positionSec = position();
    snapshot.durationSec = duration();
    snapshot.volume = volume_.load();
    snapshot.muted = muted_.load();
    snapshot.speed = static_cast<float>(speed_.load());
    snapshot.crossfading = crossfadeActive_.load();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot.uri = streams_[activeIndex_.load()].uri;
    }
    return snapshot;
}

EngineMetrics AudioEngine::metrics() const {
    EngineMetrics metrics;
    metrics.framesRendered = framesRendered_.load();
    metrics.underruns = underruns_.load();
    metrics.decodedFrames = decodedFrames_.load();
    metrics.renderCpuMsPerSecond = renderCpuMs_.load();
    metrics.bufferFill = streams_[activeIndex_.load()].ring.fill();
    return metrics;
}

void AudioEngine::levels(float* left, float* right) const {
    if (left) *left = levelLeft_.load();
    if (right) *right = levelRight_.load();
}

void AudioEngine::setState(PlaybackState state) {
    if (state_.exchange(state) == state) return;
    StateCallback callback;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callback = stateCallback_;
    }
    if (callback) callback(state);
}

// ---------------------------------------------------------------------------
// Audio callback (runs on the device thread; no locks, no allocations)
// ---------------------------------------------------------------------------
void AudioEngine::render(float* out, std::size_t frames) {
    const auto begin = std::chrono::steady_clock::now();
    const int channels = decoderOptions_.channels > 0 ? decoderOptions_.channels : 2;
    const std::size_t samples = frames * static_cast<std::size_t>(channels);
    const PlaybackState state = state_.load();

    if (state != PlaybackState::Playing && gain_ <= 0.0005f) {
        for (std::size_t i = 0; i < samples; ++i) out[i] = 0.0f;
        gain_ = 0.0f;
        levelLeft_.store(0.0f);
        levelRight_.store(0.0f);
        return;
    }

    // An offline renderer has no deadline: waiting a moment for the decoder
    // keeps exported files gapless instead of punching silence into them.
    if (offlineSink_.load() && state == PlaybackState::Playing) {
        Stream& pending = streams_[activeIndex_.load()];
        for (int spins = 0; spins < 4000; ++spins) {
            if (pending.ring.available() >= samples || pending.eof.load()) break;
            std::this_thread::sleep_for(std::chrono::microseconds(250));
        }
    }

    Stream& current = streams_[activeIndex_.load()];
    const std::size_t got = current.ring.read(out, samples);
    if (got < samples) {
        for (std::size_t i = got; i < samples; ++i) out[i] = 0.0f;
        if (state == PlaybackState::Playing && !current.eof.load() && !priming_.load()) {
            underruns_.fetch_add(1);
        }
    }

    // ---- crossfade with the preloaded stream -----------------------------
    if (crossfadeActive_.load()) {
        Stream& next = streams_[1 - activeIndex_.load()];
        const double fade = crossfade_.load();
        const double remaining = current.duration.load() - positionOf(current);
        double mix = fade > 0.0 ? 1.0 - (remaining / fade) : 0.0;
        if (mix < 0.0) mix = 0.0;
        if (mix > 1.0) mix = 1.0;
        static thread_local std::vector<float> mixBuffer;
        if (mixBuffer.size() < samples) mixBuffer.resize(samples);
        const std::size_t nextGot = next.ring.read(mixBuffer.data(), samples);
        const float inGain = static_cast<float>(std::sqrt(mix));         // equal-power fade
        const float outGain = static_cast<float>(std::sqrt(1.0 - mix));
        for (std::size_t i = 0; i < samples; ++i) {
            const float incoming = i < nextGot ? mixBuffer[i] : 0.0f;
            out[i] = out[i] * outGain + incoming * inGain;
        }
        next.consumed.fetch_add(frames);
    }

    equalizer_.process(out, frames, channels);

    // ---- ramped volume + limiter ----------------------------------------
    const float target = (state == PlaybackState::Playing) ? gainTarget_ : 0.0f;
    const float userGain = muted_.load() ? 0.0f : volume_.load();
    float peakLeft = 0.0f;
    float peakRight = 0.0f;
    for (std::size_t f = 0; f < frames; ++f) {
        gain_ += (target - gain_) * kGainSmoothing * 8.0f;
        if (gain_ > 1.0f) gain_ = 1.0f;
        if (gain_ < 0.0f) gain_ = 0.0f;
        const float g = gain_ * userGain;
        for (int c = 0; c < channels; ++c) {
            const std::size_t index = f * static_cast<std::size_t>(channels) + static_cast<std::size_t>(c);
            float value = softClip(out[index] * g);
            out[index] = value;
            value = std::fabs(value);
            if (c == 0 && value > peakLeft) peakLeft = value;
            if (c == 1 && value > peakRight) peakRight = value;
        }
    }
    if (channels == 1) peakRight = peakLeft;

    // smooth the meters so the UI does not flicker
    levelLeft_.store(std::max(peakLeft, levelLeft_.load() * 0.82f));
    levelRight_.store(std::max(peakRight, levelRight_.load() * 0.82f));

    if (state == PlaybackState::Playing || gain_ > 0.0005f) {
        current.consumed.fetch_add(frames);
        framesRendered_.fetch_add(frames);
    }

    const double elapsedMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
    const double audioSeconds = static_cast<double>(frames) / decoderOptions_.sampleRate;
    if (audioSeconds > 0) {
        const double instant = elapsedMs / audioSeconds;
        renderCpuMs_.store(renderCpuMs_.load() * 0.9 + instant * 0.1);
    }
}

// ---------------------------------------------------------------------------
// Decode thread
// ---------------------------------------------------------------------------
void AudioEngine::fillStream(Stream& stream, std::size_t chunkFrames) {
    if (!stream.decoder || stream.eof.load()) return;
    const std::size_t channels = static_cast<std::size_t>(decoderOptions_.channels);
    static thread_local std::vector<float> buffer;
    if (buffer.size() < chunkFrames * channels) buffer.resize(chunkFrames * channels);

    while (stream.ring.space() >= chunkFrames * channels) {
        const std::size_t got = stream.decoder->read(buffer.data(), chunkFrames);
        if (got == 0) {
            stream.eof.store(true);
            break;
        }
        decodedFrames_.fetch_add(got);
        stream.ring.write(buffer.data(), got * channels);
        if (got < chunkFrames && stream.decoder->eof()) {
            stream.eof.store(true);
            break;
        }
    }
}

void AudioEngine::decodeLoop() {
    while (running_.load()) {
        FinishedCallback finished;
        AdvancedCallback advanced;
        PositionCallback positionCallback;
        std::string advancedUri;
        double positionValue = 0.0;
        double durationValue = 0.0;
        bool emitFinished = false;
        bool emitAdvanced = false;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            const int activeIndex = activeIndex_.load();
            Stream& current = streams_[activeIndex];
            Stream& next = streams_[1 - activeIndex];

            // ---- seek requests -------------------------------------------
            const double seekTo = pendingSeek_.exchange(-1.0);
            if (seekTo >= 0.0 && current.decoder) {
                priming_.store(true);
                current.decoder->seek(seekTo);
                current.ring.clear();
                current.consumed.store(0);
                current.offset.store(seekTo);
                current.eof.store(false);
                crossfadeActive_.store(false);
                equalizer_.resetState();
            }

            fillStream(current, kDecodeChunk);
            priming_.store(false);

            // ---- preload the next track (gapless / crossfade) ------------
            const double totalDuration = current.duration.load();
            const double position = positionOf(current);
            const double fade = crossfade_.load();
            if (current.decoder && !next.decoder && totalDuration > 0.5 && nextUriProvider_) {
                const double trigger = std::max(fade, gapless_.load() ? 1.5 : 0.0);
                if (trigger > 0.0 && (totalDuration - position) <= trigger + 0.25) {
                    const std::string nextUri = nextUriProvider_();
                    if (!nextUri.empty()) {
                        std::string openError;
                        auto decoder = makeDecoder(nextUri, decoderOptions_, &openError);
                        if (decoder) {
                            decoder->setSpeed(speed_.load());
                            next.resetCounters();
                            next.duration.store(decoder->duration());
                            next.uri = nextUri;
                            next.decoder = std::move(decoder);
                            logDebug("engine", "preloaded " + str::fileName(nextUri));
                        } else {
                            logWarn("engine", "preload failed: " + openError);
                        }
                    }
                }
            }

            if (next.decoder) fillStream(next, kDecodeChunk);

            const bool shouldCrossfade = fade > 0.05 && next.decoder && totalDuration > 0.5 &&
                                         (totalDuration - position) <= fade &&
                                         state_.load() == PlaybackState::Playing;
            crossfadeActive_.store(shouldCrossfade);

            // ---- end of stream handling ---------------------------------
            const bool drained = current.eof.load() && current.ring.available() == 0;
            if (drained && current.decoder) {
                if (next.decoder) {
                    current.decoder.reset();
                    current.uri.clear();
                    current.resetCounters();
                    activeIndex_.store(1 - activeIndex);
                    crossfadeActive_.store(false);
                    advancedUri = next.uri;
                    advanced = advancedCallback_;
                    emitAdvanced = true;
                } else {
                    current.decoder.reset();
                    finished = finishedCallback_;
                    emitFinished = true;
                }
            }

            positionCallback = positionCallback_;
            positionValue = positionOf(streams_[activeIndex_.load()]);
            durationValue = streams_[activeIndex_.load()].duration.load();
        }

        if (emitAdvanced && advanced) advanced(advancedUri);
        if (emitFinished && finished) finished();
        if (positionCallback && std::fabs(positionValue - lastReportedPosition_) >= 0.05) {
            lastReportedPosition_ = positionValue;
            positionCallback(positionValue, durationValue);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

} // namespace aurora
