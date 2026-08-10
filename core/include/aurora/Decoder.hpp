// Aurora Player - decoding layer.
//
// Two backends, one interface:
//   * WavDecoder     - dependency free RIFF/WAVE reader (PCM 8/16/24/32, float 32/64)
//   * FfmpegDecoder  - pipes any local format or http(s) stream through ffmpeg
// A converter stage guarantees the sink format (sample rate, channels) and
// implements variable playback speed.
#pragma once

#include <cstddef>
#include <memory>
#include <string>

namespace aurora {

class IDecoder {
public:
    virtual ~IDecoder() = default;

    /// Reads interleaved float frames; returns the number of frames produced
    /// (less than `frames` only at end of stream).
    virtual std::size_t read(float* out, std::size_t frames) = 0;
    virtual bool seek(double seconds) = 0;
    virtual double duration() const = 0;   ///< 0 when unknown (live streams)
    virtual int sampleRate() const = 0;
    virtual int channels() const = 0;
    virtual bool eof() const = 0;
    virtual std::string name() const = 0;
    /// Playback rate multiplier (1.0 = normal). Ignored by raw backends.
    virtual void setSpeed(double) {}
};

struct DecoderOptions {
    int sampleRate = 48000;
    int channels = 2;
    std::string ffmpegPath = "ffmpeg";
    std::string ffprobePath = "ffprobe";
};

std::unique_ptr<IDecoder> makeWavDecoder(const std::string& path, std::string* error);
std::unique_ptr<IDecoder> makeFfmpegDecoder(const std::string& uri,
                                            const DecoderOptions& options,
                                            std::string* error);
/// Wraps `source` so that output matches `sampleRate`/`channels`; supports setSpeed().
std::unique_ptr<IDecoder> makeConverter(std::unique_ptr<IDecoder> source,
                                        int sampleRate,
                                        int channels);
/// Picks the best backend for `uri` and always returns sink-ready audio.
std::unique_ptr<IDecoder> makeDecoder(const std::string& uri,
                                      const DecoderOptions& options,
                                      std::string* error);

bool isSupportedAudioFile(const std::string& path);
bool hasFfmpeg(const std::string& ffmpegPath = "ffmpeg");
double probeDuration(const std::string& uri, const DecoderOptions& options);

} // namespace aurora
