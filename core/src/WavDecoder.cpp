#include "aurora/Decoder.hpp"

#include "aurora/Log.hpp"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

namespace aurora {
namespace {

std::uint32_t readU32(const unsigned char* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

std::uint16_t readU16(const unsigned char* p) {
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(p[0]) |
                                     (static_cast<std::uint16_t>(p[1]) << 8));
}

class WavDecoder final : public IDecoder {
public:
    bool open(const std::string& path, std::string* error) {
        file_.open(path, std::ios::binary);
        if (!file_) {
            if (error) *error = "cannot open file: " + path;
            return false;
        }
        unsigned char header[12];
        file_.read(reinterpret_cast<char*>(header), 12);
        if (file_.gcount() != 12 || std::memcmp(header, "RIFF", 4) != 0 ||
            std::memcmp(header + 8, "WAVE", 4) != 0) {
            if (error) *error = "not a RIFF/WAVE file";
            return false;
        }

        bool haveFmt = false;
        while (file_) {
            unsigned char chunk[8];
            file_.read(reinterpret_cast<char*>(chunk), 8);
            if (file_.gcount() != 8) break;
            const std::uint32_t size = readU32(chunk + 4);
            const std::streampos next =
                file_.tellg() + static_cast<std::streamoff>(size + (size & 1u));

            if (std::memcmp(chunk, "fmt ", 4) == 0) {
                std::vector<unsigned char> fmt(size < 16 ? 16 : size, 0);
                file_.read(reinterpret_cast<char*>(fmt.data()), static_cast<std::streamsize>(size));
                formatTag_ = readU16(fmt.data());
                channels_ = readU16(fmt.data() + 2);
                sampleRate_ = static_cast<int>(readU32(fmt.data() + 4));
                bitsPerSample_ = readU16(fmt.data() + 14);
                if (formatTag_ == 0xFFFE && size >= 40) {  // WAVE_FORMAT_EXTENSIBLE
                    formatTag_ = readU16(fmt.data() + 24);  // first two bytes of the sub GUID
                }
                haveFmt = channels_ > 0 && sampleRate_ > 0 && bitsPerSample_ > 0;
            } else if (std::memcmp(chunk, "data", 4) == 0) {
                dataOffset_ = file_.tellg();
                dataBytes_ = size;
                if (haveFmt) break;
            }
            file_.seekg(next);
        }

        if (!haveFmt || dataBytes_ == 0) {
            if (error) *error = "unsupported or truncated WAV file";
            return false;
        }
        if (formatTag_ != 1 && formatTag_ != 3) {
            if (error) *error = "unsupported WAV codec (only PCM and IEEE float)";
            return false;
        }
        bytesPerSample_ = bitsPerSample_ / 8;
        frameBytes_ = bytesPerSample_ * channels_;
        if (frameBytes_ == 0) {
            if (error) *error = "invalid WAV frame size";
            return false;
        }
        totalFrames_ = dataBytes_ / frameBytes_;
        file_.clear();
        file_.seekg(dataOffset_);
        framePos_ = 0;
        return true;
    }

    std::size_t read(float* out, std::size_t frames) override {
        if (framePos_ >= totalFrames_) return 0;
        const std::size_t remaining = totalFrames_ - framePos_;
        const std::size_t want = frames < remaining ? frames : remaining;
        scratch_.resize(want * frameBytes_);
        file_.read(reinterpret_cast<char*>(scratch_.data()),
                   static_cast<std::streamsize>(scratch_.size()));
        const std::size_t got = static_cast<std::size_t>(file_.gcount()) / frameBytes_;
        if (file_.eof()) file_.clear();

        const unsigned char* src = scratch_.data();
        const std::size_t samples = got * static_cast<std::size_t>(channels_);
        for (std::size_t i = 0; i < samples; ++i) {
            const unsigned char* p = src + i * static_cast<std::size_t>(bytesPerSample_);
            float value = 0.0f;
            if (formatTag_ == 3) {
                if (bitsPerSample_ == 32) {
                    float tmp;
                    std::memcpy(&tmp, p, 4);
                    value = tmp;
                } else if (bitsPerSample_ == 64) {
                    double tmp;
                    std::memcpy(&tmp, p, 8);
                    value = static_cast<float>(tmp);
                }
            } else {
                switch (bitsPerSample_) {
                    case 8:
                        value = (static_cast<float>(p[0]) - 128.0f) / 128.0f;
                        break;
                    case 16: {
                        const std::int16_t tmp = static_cast<std::int16_t>(readU16(p));
                        value = static_cast<float>(tmp) / 32768.0f;
                        break;
                    }
                    case 24: {
                        std::int32_t tmp = (static_cast<std::int32_t>(p[0]) << 8) |
                                           (static_cast<std::int32_t>(p[1]) << 16) |
                                           (static_cast<std::int32_t>(p[2]) << 24);
                        value = static_cast<float>(tmp >> 8) / 8388608.0f;
                        break;
                    }
                    case 32: {
                        const std::int32_t tmp = static_cast<std::int32_t>(readU32(p));
                        value = static_cast<float>(tmp) / 2147483648.0f;
                        break;
                    }
                    default:
                        value = 0.0f;
                }
            }
            out[i] = value;
        }
        framePos_ += got;
        return got;
    }

    bool seek(double seconds) override {
        if (seconds < 0) seconds = 0;
        std::size_t frame = static_cast<std::size_t>(seconds * sampleRate_);
        if (frame > totalFrames_) frame = totalFrames_;
        file_.clear();
        file_.seekg(dataOffset_ + static_cast<std::streamoff>(frame * frameBytes_));
        framePos_ = frame;
        return true;
    }

    double duration() const override {
        return sampleRate_ > 0 ? static_cast<double>(totalFrames_) / sampleRate_ : 0.0;
    }
    int sampleRate() const override { return sampleRate_; }
    int channels() const override { return channels_; }
    bool eof() const override { return framePos_ >= totalFrames_; }
    std::string name() const override { return "wav"; }

private:
    std::ifstream file_;
    std::streampos dataOffset_{0};
    std::uint32_t dataBytes_ = 0;
    std::uint16_t formatTag_ = 0;
    int channels_ = 0;
    int sampleRate_ = 0;
    std::uint16_t bitsPerSample_ = 0;
    int bytesPerSample_ = 0;
    std::size_t frameBytes_ = 0;
    std::size_t totalFrames_ = 0;
    std::size_t framePos_ = 0;
    std::vector<unsigned char> scratch_;
};

} // namespace

std::unique_ptr<IDecoder> makeWavDecoder(const std::string& path, std::string* error) {
    auto decoder = std::unique_ptr<WavDecoder>(new WavDecoder());
    if (!decoder->open(path, error)) return nullptr;
    return std::unique_ptr<IDecoder>(decoder.release());
}

} // namespace aurora
