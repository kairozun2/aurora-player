#include "aurora/Decoder.hpp"

#include "aurora/Log.hpp"
#include "aurora/Process.hpp"
#include "aurora/Strings.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#ifdef _WIN32
#define AURORA_POPEN _popen
#define AURORA_PCLOSE _pclose
#else
#define AURORA_POPEN popen
#define AURORA_PCLOSE pclose
#endif

namespace aurora {
namespace {

/// Universal decoder: ffmpeg converts anything (mp3/flac/aac/ogg/opus/m4a/wv,
/// local files and http streams) into raw float PCM on stdout.
class FfmpegDecoder final : public IDecoder {
public:
    FfmpegDecoder(std::string uri, DecoderOptions options)
        : uri_(std::move(uri)), options_(std::move(options)) {}

    ~FfmpegDecoder() override { closePipe(); }

    bool open(std::string* error) {
        duration_ = probeDuration(uri_, options_);
        if (!openPipe(0.0)) {
            if (error) *error = "ffmpeg could not open: " + uri_;
            return false;
        }
        return true;
    }

    std::size_t read(float* out, std::size_t frames) override {
        if (!pipe_) return 0;
        const std::size_t samples = frames * static_cast<std::size_t>(options_.channels);
        const std::size_t got = std::fread(out, sizeof(float), samples, pipe_);
        if (got < samples) {
            if (std::feof(pipe_)) eof_ = true;
        }
        const std::size_t framesRead = got / static_cast<std::size_t>(options_.channels);
        framePos_ += framesRead;
        return framesRead;
    }

    bool seek(double seconds) override {
        if (seconds < 0) seconds = 0;
        closePipe();
        eof_ = false;
        if (!openPipe(seconds)) return false;
        framePos_ = static_cast<std::size_t>(seconds * options_.sampleRate);
        return true;
    }

    double duration() const override { return duration_; }
    int sampleRate() const override { return options_.sampleRate; }
    int channels() const override { return options_.channels; }
    bool eof() const override { return eof_; }
    std::string name() const override { return "ffmpeg"; }

private:
    bool openPipe(double startSeconds) {
        std::vector<std::string> args;
        args.push_back(options_.ffmpegPath);
        args.push_back("-hide_banner");
        args.push_back("-loglevel");
        args.push_back("error");
        args.push_back("-nostdin");
        if (str::isHttpUrl(uri_)) {
            // Be resilient on flaky networks when streaming online sources.
            args.push_back("-reconnect");
            args.push_back("1");
            args.push_back("-reconnect_streamed");
            args.push_back("1");
            args.push_back("-reconnect_delay_max");
            args.push_back("5");
        }
        if (startSeconds > 0.0) {
            args.push_back("-ss");
            args.push_back(str::formatDouble(startSeconds, 3));
        }
        args.push_back("-i");
        args.push_back(uri_);
        args.push_back("-vn");
        args.push_back("-map");
        args.push_back("a:0");
        args.push_back("-f");
        args.push_back("f32le");
        args.push_back("-acodec");
        args.push_back("pcm_f32le");
        args.push_back("-ac");
        args.push_back(std::to_string(options_.channels));
        args.push_back("-ar");
        args.push_back(std::to_string(options_.sampleRate));
        args.push_back("-");

        std::string cmd = Process::commandLine(args);
#ifdef _WIN32
        cmd += " 2>NUL";
        pipe_ = AURORA_POPEN(cmd.c_str(), "rb");
#else
        cmd += " 2>/dev/null";
        pipe_ = AURORA_POPEN(cmd.c_str(), "r");
#endif
        if (!pipe_) {
            logError("decoder", "failed to spawn ffmpeg");
            return false;
        }
        return true;
    }

    void closePipe() {
        if (pipe_) {
            AURORA_PCLOSE(pipe_);
            pipe_ = nullptr;
        }
    }

    std::string uri_;
    DecoderOptions options_;
    std::FILE* pipe_ = nullptr;
    double duration_ = 0.0;
    std::size_t framePos_ = 0;
    bool eof_ = false;
};

} // namespace

bool hasFfmpeg(const std::string& ffmpegPath) {
    static bool checked = false;
    static bool cached = false;
    static std::string cachedPath;
    if (checked && cachedPath == ffmpegPath) return cached;
    cached = Process::exists(ffmpegPath);
    cachedPath = ffmpegPath;
    checked = true;
    return cached;
}

double probeDuration(const std::string& uri, const DecoderOptions& options) {
    std::vector<std::string> args;
    args.push_back(options.ffprobePath);
    args.push_back("-v");
    args.push_back("error");
    args.push_back("-show_entries");
    args.push_back("format=duration");
    args.push_back("-of");
    args.push_back("default=nw=1:nk=1");
    args.push_back(uri);
    const Process::Result result = Process::run(args);
    if (!result.ok()) return 0.0;
    const std::string text = str::trim(result.output);
    if (text.empty() || text == "N/A") return 0.0;
    const double value = std::strtod(text.c_str(), nullptr);
    return value > 0.0 ? value : 0.0;
}

std::unique_ptr<IDecoder> makeFfmpegDecoder(const std::string& uri,
                                            const DecoderOptions& options,
                                            std::string* error) {
    if (!hasFfmpeg(options.ffmpegPath)) {
        if (error) *error = "ffmpeg is not installed";
        return nullptr;
    }
    auto decoder = std::unique_ptr<FfmpegDecoder>(new FfmpegDecoder(uri, options));
    if (!decoder->open(error)) return nullptr;
    return std::unique_ptr<IDecoder>(decoder.release());
}

} // namespace aurora
