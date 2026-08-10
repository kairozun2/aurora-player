// Aurora Player - audio output abstraction.
//
// The engine never talks to a device directly: it fills buffers when the sink
// asks for them. This keeps the same engine usable for a real device (Qt),
// for headless/CI runs (NullSink) and for offline rendering (WavFileSink).
#pragma once

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>

namespace aurora {

class ISink {
public:
    using RenderCallback = std::function<void(float* out, std::size_t frames)>;

    virtual ~ISink() = default;

    virtual bool start(int sampleRate, int channels, std::string* error) = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
    virtual int sampleRate() const = 0;
    virtual int channels() const = 0;
    virtual std::string name() const = 0;

    /// True for offline renderers (file export). Such sinks are not bound to a
    /// device clock, so the engine waits for the decoder instead of writing
    /// silence into the output file.
    virtual bool offline() const { return false; }

    void setRenderCallback(RenderCallback callback) { render_ = std::move(callback); }

protected:
    void renderInto(float* out, std::size_t frames) {
        if (render_) {
            render_(out, frames);
        } else {
            for (std::size_t i = 0; i < frames; ++i) out[i] = 0.0f;
        }
    }

    RenderCallback render_;
};

/// Silent device-free sink that keeps real time with a background thread.
/// `speedFactor` > 1 makes it run faster than real time (used by tests).
std::unique_ptr<ISink> makeNullSink(double speedFactor = 1.0);

/// Renders the engine output into a 16-bit WAV file as fast as the CPU allows.
std::unique_ptr<ISink> makeWavFileSink(const std::string& path, double maxSeconds);

} // namespace aurora
