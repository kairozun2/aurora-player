#include "aurora/TagReader.hpp"

#include "aurora/Json.hpp"
#include "aurora/Log.hpp"
#include "aurora/Process.hpp"
#include "aurora/Strings.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>

namespace aurora {
namespace {

using Bytes = std::vector<unsigned char>;

Bytes readFilePart(const std::string& path, std::size_t maxBytes) {
    Bytes data;
    std::ifstream in(path, std::ios::binary);
    if (!in) return data;
    data.resize(maxBytes);
    in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(maxBytes));
    data.resize(static_cast<std::size_t>(in.gcount()));
    return data;
}

std::uint64_t fileSizeOf(const std::string& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return 0;
    return static_cast<std::uint64_t>(in.tellg());
}

void appendUtf8(std::string& out, unsigned int cp) {
    if (cp <= 0x7F) {
        out += static_cast<char>(cp);
    } else if (cp <= 0x7FF) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp <= 0xFFFF) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

/// Windows-1251 -> UTF-8 (Russian legacy tags).
std::string cp1251ToUtf8(const std::string& in) {
    static const unsigned short kHigh[128] = {
        0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021, 0x20AC, 0x2030,
        0x0409, 0x2039, 0x040A, 0x040C, 0x040B, 0x040F, 0x0452, 0x2018, 0x2019, 0x201C,
        0x201D, 0x2022, 0x2013, 0x2014, 0x0098, 0x2122, 0x0459, 0x203A, 0x045A, 0x045C,
        0x045B, 0x045F, 0x00A0, 0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6, 0x00A7,
        0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407, 0x00B0, 0x00B1,
        0x0406, 0x0456, 0x0491, 0x00B5, 0x00B6, 0x00B7, 0x0451, 0x2116, 0x0454, 0x00BB,
        0x0458, 0x0405, 0x0455, 0x0457, 0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415,
        0x0416, 0x0417, 0x0418, 0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E, 0x041F,
        0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427, 0x0428, 0x0429,
        0x042A, 0x042B, 0x042C, 0x042D, 0x042E, 0x042F, 0x0430, 0x0431, 0x0432, 0x0433,
        0x0434, 0x0435, 0x0436, 0x0437, 0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D,
        0x043E, 0x043F, 0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447,
        0x0448, 0x0449, 0x044A, 0x044B, 0x044C, 0x044D, 0x044E, 0x044F};
    std::string out;
    out.reserve(in.size() * 2);
    for (const unsigned char c : in) {
        if (c < 0x80) {
            out += static_cast<char>(c);
        } else {
            appendUtf8(out, kHigh[c - 0x80]);
        }
    }
    return out;
}

std::string latin1ToUtf8(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 2);
    for (const unsigned char c : in) appendUtf8(out, c);
    return out;
}

/// Old Russian taggers wrote CP1251 bytes into "ISO-8859-1" frames. Detect the
/// typical Cyrillic byte range and decode accordingly.
bool looksLikeCp1251(const std::string& in) {
    int cyrillic = 0;
    int other = 0;
    for (const unsigned char c : in) {
        if (c >= 0xC0) ++cyrillic;
        else if (c >= 0x80) ++other;
    }
    return cyrillic > 0 && cyrillic >= other;
}

std::string decodeText(const std::string& raw, int encoding) {
    switch (encoding) {
        case 0: {  // ISO-8859-1 (or CP1251 in the wild)
            return looksLikeCp1251(raw) ? cp1251ToUtf8(raw) : latin1ToUtf8(raw);
        }
        case 1: {  // UTF-16 with BOM
            std::string out;
            if (raw.size() < 2) return out;
            const bool bigEndian = static_cast<unsigned char>(raw[0]) == 0xFE;
            for (std::size_t i = 2; i + 1 < raw.size(); i += 2) {
                const unsigned int a = static_cast<unsigned char>(raw[i]);
                const unsigned int b = static_cast<unsigned char>(raw[i + 1]);
                unsigned int cp = bigEndian ? ((a << 8) | b) : ((b << 8) | a);
                if (cp == 0) break;
                appendUtf8(out, cp);
            }
            return out;
        }
        case 2: {  // UTF-16BE
            std::string out;
            for (std::size_t i = 0; i + 1 < raw.size(); i += 2) {
                const unsigned int cp = (static_cast<unsigned char>(raw[i]) << 8) |
                                        static_cast<unsigned char>(raw[i + 1]);
                if (cp == 0) break;
                appendUtf8(out, cp);
            }
            return out;
        }
        default:
            return raw;  // UTF-8
    }
}

std::string trimNulls(const std::string& in) {
    std::string out = in;
    while (!out.empty() && (out[out.size() - 1] == '\0' || out[out.size() - 1] == ' ')) {
        out.erase(out.size() - 1);
    }
    return str::trim(out);
}

int parseLeadingInt(const std::string& in) {
    return static_cast<int>(std::strtol(in.c_str(), nullptr, 10));
}

std::uint32_t beU32(const unsigned char* p) {
    return (static_cast<std::uint32_t>(p[0]) << 24) | (static_cast<std::uint32_t>(p[1]) << 16) |
           (static_cast<std::uint32_t>(p[2]) << 8) | static_cast<std::uint32_t>(p[3]);
}

std::uint32_t leU32(const unsigned char* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

std::uint32_t syncSafe(const unsigned char* p) {
    return (static_cast<std::uint32_t>(p[0] & 0x7F) << 21) |
           (static_cast<std::uint32_t>(p[1] & 0x7F) << 14) |
           (static_cast<std::uint32_t>(p[2] & 0x7F) << 7) |
           static_cast<std::uint32_t>(p[3] & 0x7F);
}

// ---------------------------------------------------------------- ID3 -------
bool parseId3v2(const Bytes& data, Tags* tags, Bytes* cover, std::string* coverMime) {
    if (data.size() < 10 || std::memcmp(data.data(), "ID3", 3) != 0) return false;
    const int major = data[3];
    const std::uint32_t tagSize = syncSafe(data.data() + 6);
    std::size_t offset = 10;
    if (data[5] & 0x40) {  // extended header
        if (offset + 4 > data.size()) return false;
        offset += (major >= 4) ? syncSafe(data.data() + offset) : beU32(data.data() + offset) + 4;
    }
    const std::size_t end = std::min<std::size_t>(data.size(), 10 + tagSize);
    const int idSize = (major == 2) ? 3 : 4;
    const int headerSize = (major == 2) ? 6 : 10;

    while (offset + static_cast<std::size_t>(headerSize) <= end) {
        const unsigned char* p = data.data() + offset;
        if (p[0] == 0) break;  // padding
        std::string id(reinterpret_cast<const char*>(p), static_cast<std::size_t>(idSize));
        std::uint32_t size = 0;
        if (major == 2) {
            size = (static_cast<std::uint32_t>(p[3]) << 16) |
                   (static_cast<std::uint32_t>(p[4]) << 8) | static_cast<std::uint32_t>(p[5]);
        } else if (major == 4) {
            size = syncSafe(p + 4);
        } else {
            size = beU32(p + 4);
        }
        const std::size_t payload = offset + static_cast<std::size_t>(headerSize);
        if (size == 0 || payload + size > end) break;

        const bool isPicture = (id == "APIC" || id == "PIC");
        if (isPicture) {
            std::size_t i = payload;
            const int encoding = data[i++];
            std::string mime;
            if (id == "PIC") {
                mime = "image/" + str::toLower(std::string(reinterpret_cast<const char*>(data.data() + i), 3));
                i += 3;
            } else {
                while (i < payload + size && data[i] != 0) mime += static_cast<char>(data[i++]);
                ++i;  // null terminator
            }
            if (i < payload + size) ++i;  // picture type
            // skip description
            if (encoding == 1 || encoding == 2) {
                while (i + 1 < payload + size && !(data[i] == 0 && data[i + 1] == 0)) i += 2;
                i += 2;
            } else {
                while (i < payload + size && data[i] != 0) ++i;
                ++i;
            }
            if (i < payload + size) {
                tags->hasCover = true;
                tags->coverMime = mime.empty() ? "image/jpeg" : mime;
                if (cover) cover->assign(data.begin() + static_cast<long>(i),
                                         data.begin() + static_cast<long>(payload + size));
                if (coverMime) *coverMime = tags->coverMime;
            }
        } else if (id[0] == 'T' && size >= 1) {
            const int encoding = data[payload];
            const std::string raw(reinterpret_cast<const char*>(data.data() + payload + 1),
                                  static_cast<std::size_t>(size - 1));
            const std::string value = trimNulls(decodeText(raw, encoding));
            if (id == "TIT2" || id == "TT2") tags->title = value;
            else if (id == "TPE1" || id == "TP1") tags->artist = value;
            else if (id == "TALB" || id == "TAL") tags->album = value;
            else if (id == "TPE2" || id == "TP2") tags->albumArtist = value;
            else if (id == "TCON" || id == "TCO") tags->genre = value;
            else if (id == "TCOM" || id == "TCM") tags->composer = value;
            else if (id == "TYER" || id == "TYE" || id == "TDRC") {
                if (tags->year.empty()) tags->year = value.substr(0, 4);
            } else if (id == "TRCK" || id == "TRK") tags->trackNo = parseLeadingInt(value);
            else if (id == "TPOS" || id == "TPA") tags->discNo = parseLeadingInt(value);
        } else if ((id == "COMM" || id == "COM") && size > 4) {
            const int encoding = data[payload];
            std::size_t i = payload + 4;  // encoding + language
            if (encoding == 1 || encoding == 2) {
                while (i + 1 < payload + size && !(data[i] == 0 && data[i + 1] == 0)) i += 2;
                i += 2;
            } else {
                while (i < payload + size && data[i] != 0) ++i;
                ++i;
            }
            if (i < payload + size) {
                const std::string raw(reinterpret_cast<const char*>(data.data() + i),
                                      payload + size - i);
                tags->comment = trimNulls(decodeText(raw, encoding));
            }
        }
        offset = payload + size;
    }
    tags->parser = "id3v2";
    return true;
}

bool parseId3v1(const std::string& path, Tags* tags) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return false;
    const std::streamoff size = in.tellg();
    if (size < 128) return false;
    in.seekg(size - 128);
    char buffer[128];
    in.read(buffer, 128);
    if (std::memcmp(buffer, "TAG", 3) != 0) return false;
    auto field = [&](int offset, int length) {
        std::string raw(buffer + offset, static_cast<std::size_t>(length));
        raw = trimNulls(raw);
        return looksLikeCp1251(raw) ? cp1251ToUtf8(raw) : latin1ToUtf8(raw);
    };
    if (tags->title.empty()) tags->title = field(3, 30);
    if (tags->artist.empty()) tags->artist = field(33, 30);
    if (tags->album.empty()) tags->album = field(63, 30);
    if (tags->year.empty()) tags->year = field(93, 4);
    if (tags->trackNo == 0) tags->trackNo = static_cast<unsigned char>(buffer[126]);
    if (tags->parser.empty()) tags->parser = "id3v1";
    return true;
}

/// Estimates duration from the first MPEG audio frame header + file size.
void estimateMp3Duration(const std::string& path, const Bytes& head, Tags* tags) {
    static const int kBitrates[16] = {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0};
    static const int kRates[4] = {44100, 48000, 32000, 0};
    for (std::size_t i = 0; i + 3 < head.size(); ++i) {
        if (head[i] != 0xFF || (head[i + 1] & 0xE0) != 0xE0) continue;
        const int versionBits = (head[i + 1] >> 3) & 0x03;
        const int layer = (head[i + 1] >> 1) & 0x03;
        if (layer != 1) continue;  // Layer III only
        const int bitrateIndex = (head[i + 2] >> 4) & 0x0F;
        const int rateIndex = (head[i + 2] >> 2) & 0x03;
        const int mode = (head[i + 3] >> 6) & 0x03;
        if (kBitrates[bitrateIndex] == 0 || kRates[rateIndex] == 0) continue;
        int rate = kRates[rateIndex];
        if (versionBits == 2) rate /= 2;        // MPEG2
        else if (versionBits == 0) rate /= 4;   // MPEG2.5
        tags->bitrateKbps = kBitrates[bitrateIndex];
        tags->sampleRate = rate;
        tags->channels = (mode == 3) ? 1 : 2;
        const std::uint64_t size = fileSizeOf(path);
        if (size > 0 && tags->durationSec <= 0.0) {
            tags->durationSec = static_cast<double>(size * 8) /
                                (static_cast<double>(tags->bitrateKbps) * 1000.0);
        }
        return;
    }
}

// --------------------------------------------------------------- FLAC ------
void applyVorbisComment(const std::string& field, Tags* tags) {
    const std::size_t eq = field.find('=');
    if (eq == std::string::npos) return;
    const std::string key = str::toLower(field.substr(0, eq));
    const std::string value = str::trim(field.substr(eq + 1));
    if (key == "title") tags->title = value;
    else if (key == "artist") tags->artist = value;
    else if (key == "album") tags->album = value;
    else if (key == "albumartist" || key == "album artist") tags->albumArtist = value;
    else if (key == "genre") tags->genre = value;
    else if (key == "date" || key == "year") tags->year = value.substr(0, 4);
    else if (key == "tracknumber") tags->trackNo = parseLeadingInt(value);
    else if (key == "discnumber") tags->discNo = parseLeadingInt(value);
    else if (key == "comment" || key == "description") tags->comment = value;
    else if (key == "composer") tags->composer = value;
}

void parseVorbisCommentBlock(const unsigned char* data, std::size_t size, Tags* tags) {
    std::size_t i = 0;
    if (i + 4 > size) return;
    const std::uint32_t vendorLength = leU32(data + i);
    i += 4 + vendorLength;
    if (i + 4 > size) return;
    const std::uint32_t count = leU32(data + i);
    i += 4;
    for (std::uint32_t c = 0; c < count && i + 4 <= size; ++c) {
        const std::uint32_t length = leU32(data + i);
        i += 4;
        if (i + length > size) break;
        applyVorbisComment(std::string(reinterpret_cast<const char*>(data + i), length), tags);
        i += length;
    }
    if (tags->parser.empty()) tags->parser = "vorbis";
}

bool parseFlac(const std::string& path, Tags* tags, Bytes* cover, std::string* coverMime) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    char magic[4];
    in.read(magic, 4);
    if (std::memcmp(magic, "fLaC", 4) != 0) return false;

    while (in) {
        unsigned char header[4];
        in.read(reinterpret_cast<char*>(header), 4);
        if (in.gcount() != 4) break;
        const bool last = (header[0] & 0x80) != 0;
        const int type = header[0] & 0x7F;
        const std::uint32_t length = (static_cast<std::uint32_t>(header[1]) << 16) |
                                     (static_cast<std::uint32_t>(header[2]) << 8) |
                                     static_cast<std::uint32_t>(header[3]);
        Bytes block(length);
        in.read(reinterpret_cast<char*>(block.data()), static_cast<std::streamsize>(length));
        if (static_cast<std::uint32_t>(in.gcount()) != length) break;

        if (type == 0 && length >= 18) {  // STREAMINFO
            const std::uint32_t rate = (static_cast<std::uint32_t>(block[10]) << 12) |
                                       (static_cast<std::uint32_t>(block[11]) << 4) |
                                       (static_cast<std::uint32_t>(block[12]) >> 4);
            const int channels = ((block[12] >> 1) & 0x07) + 1;
            std::uint64_t totalSamples = (static_cast<std::uint64_t>(block[13] & 0x0F) << 32) |
                                         (static_cast<std::uint64_t>(block[14]) << 24) |
                                         (static_cast<std::uint64_t>(block[15]) << 16) |
                                         (static_cast<std::uint64_t>(block[16]) << 8) |
                                         static_cast<std::uint64_t>(block[17]);
            tags->sampleRate = static_cast<int>(rate);
            tags->channels = channels;
            if (rate > 0 && totalSamples > 0) {
                tags->durationSec = static_cast<double>(totalSamples) / static_cast<double>(rate);
            }
        } else if (type == 4) {  // VORBIS_COMMENT
            parseVorbisCommentBlock(block.data(), block.size(), tags);
        } else if (type == 6 && length > 32) {  // PICTURE
            std::size_t i = 4;  // picture type
            const std::uint32_t mimeLength = beU32(block.data() + i);
            i += 4;
            std::string mime(reinterpret_cast<const char*>(block.data() + i), mimeLength);
            i += mimeLength;
            const std::uint32_t descLength = beU32(block.data() + i);
            i += 4 + descLength;
            i += 16;  // width, height, depth, colors
            if (i + 4 <= block.size()) {
                const std::uint32_t dataLength = beU32(block.data() + i);
                i += 4;
                if (i + dataLength <= block.size()) {
                    tags->hasCover = true;
                    tags->coverMime = mime;
                    if (cover) cover->assign(block.begin() + static_cast<long>(i),
                                             block.begin() + static_cast<long>(i + dataLength));
                    if (coverMime) *coverMime = mime;
                }
            }
        }
        if (last) break;
    }
    tags->parser = "flac";
    return true;
}

// ---------------------------------------------------------------- MP4 ------
void parseIlst(const Bytes& data, std::size_t begin, std::size_t end, Tags* tags,
               Bytes* cover, std::string* coverMime) {
    std::size_t i = begin;
    while (i + 8 <= end) {
        const std::uint32_t size = beU32(data.data() + i);
        if (size < 8 || i + size > end) break;
        const std::string name(reinterpret_cast<const char*>(data.data() + i + 4), 4);
        // entry = 4 size + 4 name, then a 'data' box = 4 size + 4 'data' +
        // 4 type + 4 locale, so the value itself starts 24 bytes in.
        if (size >= 24) {
            const std::size_t dataStart = i + 24;
            const std::size_t payload = i + size;
            if (dataStart < payload) {
                const std::string value(reinterpret_cast<const char*>(data.data() + dataStart),
                                        payload - dataStart);
                if (name == "\xA9nam") tags->title = str::trim(value);
                else if (name == "\xA9" "ART") tags->artist = str::trim(value);
                else if (name == "\xA9" "alb") tags->album = str::trim(value);
                else if (name == "aART") tags->albumArtist = str::trim(value);
                else if (name == "\xA9gen") tags->genre = str::trim(value);
                else if (name == "\xA9" "day") tags->year = str::trim(value).substr(0, 4);
                else if (name == "\xA9" "cmt") tags->comment = str::trim(value);
                else if (name == "trkn" && value.size() >= 4) {
                    tags->trackNo = static_cast<unsigned char>(value[3]);
                } else if (name == "disk" && value.size() >= 4) {
                    tags->discNo = static_cast<unsigned char>(value[3]);
                } else if (name == "covr") {
                    tags->hasCover = true;
                    tags->coverMime = (value.size() > 3 && static_cast<unsigned char>(value[0]) == 0x89)
                                          ? "image/png" : "image/jpeg";
                    if (cover) cover->assign(data.begin() + static_cast<long>(dataStart),
                                             data.begin() + static_cast<long>(payload));
                    if (coverMime) *coverMime = tags->coverMime;
                }
            }
        }
        i += size;
    }
    tags->parser = "mp4";
}

void walkMp4(const Bytes& data, std::size_t begin, std::size_t end, Tags* tags,
             Bytes* cover, std::string* coverMime, int depth) {
    if (depth > 6) return;
    std::size_t i = begin;
    while (i + 8 <= end) {
        std::uint64_t size = beU32(data.data() + i);
        const std::string name(reinterpret_cast<const char*>(data.data() + i + 4), 4);
        std::size_t headerSize = 8;
        if (size == 1 && i + 16 <= end) {  // 64-bit size
            size = (static_cast<std::uint64_t>(beU32(data.data() + i + 8)) << 32) |
                   beU32(data.data() + i + 12);
            headerSize = 16;
        }
        if (size < headerSize || i + size > end) break;
        if (name == "moov" || name == "udta" || name == "trak" || name == "mdia") {
            walkMp4(data, i + headerSize, i + static_cast<std::size_t>(size), tags, cover, coverMime, depth + 1);
        } else if (name == "meta") {
            // 'meta' has a 4 byte version/flags field before its children
            walkMp4(data, i + headerSize + 4, i + static_cast<std::size_t>(size), tags, cover, coverMime, depth + 1);
        } else if (name == "ilst") {
            parseIlst(data, i + headerSize, i + static_cast<std::size_t>(size), tags, cover, coverMime);
        }
        i += static_cast<std::size_t>(size);
    }
}

// ---------------------------------------------------------------- WAV ------
bool parseWavInfo(const std::string& path, Tags* tags) {
    const Bytes data = readFilePart(path, 1 << 16);
    if (data.size() < 12 || std::memcmp(data.data(), "RIFF", 4) != 0) return false;
    std::size_t i = 12;
    std::uint32_t byteRate = 0;
    while (i + 8 <= data.size()) {
        const std::string id(reinterpret_cast<const char*>(data.data() + i), 4);
        const std::uint32_t size = leU32(data.data() + i + 4);
        const std::size_t payload = i + 8;
        if (id == "fmt " && payload + 16 <= data.size()) {
            tags->channels = data[payload + 2] | (data[payload + 3] << 8);
            tags->sampleRate = static_cast<int>(leU32(data.data() + payload + 4));
            byteRate = leU32(data.data() + payload + 8);
        } else if (id == "data") {
            if (byteRate > 0) tags->durationSec = static_cast<double>(size) / byteRate;
            if (tags->sampleRate > 0) tags->bitrateKbps = static_cast<int>(byteRate * 8 / 1000);
        } else if (id == "LIST" && payload + 4 <= data.size() &&
                   std::memcmp(data.data() + payload, "INFO", 4) == 0) {
            std::size_t j = payload + 4;
            while (j + 8 <= payload + size && j + 8 <= data.size()) {
                const std::string key(reinterpret_cast<const char*>(data.data() + j), 4);
                const std::uint32_t length = leU32(data.data() + j + 4);
                if (j + 8 + length > data.size()) break;
                std::string value(reinterpret_cast<const char*>(data.data() + j + 8), length);
                value = trimNulls(value);
                if (looksLikeCp1251(value)) value = cp1251ToUtf8(value);
                if (key == "INAM") tags->title = value;
                else if (key == "IART") tags->artist = value;
                else if (key == "IPRD") tags->album = value;
                else if (key == "IGNR") tags->genre = value;
                else if (key == "ICRD") tags->year = value.substr(0, 4);
                else if (key == "ITRK") tags->trackNo = parseLeadingInt(value);
                else if (key == "ICMT") tags->comment = value;
                j += 8 + length + (length & 1u);
            }
        }
        i = payload + size + (size & 1u);
    }
    tags->parser = "wav";
    return true;
}

// --------------------------------------------------------- Ogg / Opus ------
bool parseOgg(const std::string& path, Tags* tags) {
    const Bytes data = readFilePart(path, 1 << 18);
    if (data.size() < 4 || std::memcmp(data.data(), "OggS", 4) != 0) return false;
    static const char kVorbis[] = {0x03, 'v', 'o', 'r', 'b', 'i', 's'};
    for (std::size_t i = 0; i + sizeof(kVorbis) < data.size(); ++i) {
        if (std::memcmp(data.data() + i, kVorbis, sizeof(kVorbis)) == 0) {
            parseVorbisCommentBlock(data.data() + i + sizeof(kVorbis),
                                    data.size() - i - sizeof(kVorbis), tags);
            tags->parser = "ogg";
            return true;
        }
        if (std::memcmp(data.data() + i, "OpusTags", 8) == 0) {
            parseVorbisCommentBlock(data.data() + i + 8, data.size() - i - 8, tags);
            tags->parser = "opus";
            return true;
        }
    }
    return false;
}

} // namespace

Tags TagReader::fromFileName(const std::string& path) {
    Tags tags;
    std::string stem = str::stem(path);
    stem = str::replaceAll(stem, "_", " ");

    // leading track number: "01 - ", "01. ", "01 "
    std::size_t i = 0;
    while (i < stem.size() && std::isdigit(static_cast<unsigned char>(stem[i]))) ++i;
    if (i > 0 && i <= 3 && i < stem.size()) {
        tags.trackNo = parseLeadingInt(stem.substr(0, i));
        while (i < stem.size() && (stem[i] == ' ' || stem[i] == '.' || stem[i] == '-')) ++i;
        stem = stem.substr(i);
    }

    const std::size_t dash = stem.find(" - ");
    if (dash != std::string::npos) {
        tags.artist = str::trim(stem.substr(0, dash));
        tags.title = str::trim(stem.substr(dash + 3));
    } else {
        tags.title = str::trim(stem);
    }
    tags.parser = "filename";
    return tags;
}

bool TagReader::readWithFfprobe(const std::string& path, Tags* out, const std::string& ffprobePath) {
    if (!out) return false;
    std::vector<std::string> args;
    args.push_back(ffprobePath);
    args.push_back("-v");
    args.push_back("error");
    args.push_back("-show_format");
    args.push_back("-show_streams");
    args.push_back("-select_streams");
    args.push_back("a:0");
    args.push_back("-of");
    args.push_back("json");
    args.push_back(path);
    const Process::Result result = Process::run(args);
    if (!result.ok()) return false;

    std::string error;
    const Json root = Json::parse(result.output, &error);
    if (!error.empty()) return false;

    const Json& format = root["format"];
    if (out->durationSec <= 0.0) out->durationSec = format["duration"].asDouble(0.0);
    if (out->bitrateKbps <= 0) {
        out->bitrateKbps = static_cast<int>(format["bit_rate"].asDouble(0.0) / 1000.0);
    }

    const Json& streams = root["streams"];
    if (streams.size() > 0) {
        const Json& stream = streams.at(0);
        if (out->sampleRate <= 0) out->sampleRate = static_cast<int>(stream["sample_rate"].asDouble(0.0));
        if (out->channels <= 0) out->channels = static_cast<int>(stream["channels"].asDouble(0.0));
        if (out->durationSec <= 0.0) out->durationSec = stream["duration"].asDouble(0.0);
    }

    const Json& tagsNode = format["tags"];
    auto pick = [&](const char* a, const char* b) {
        std::string value = tagsNode[a].asString();
        if (value.empty()) value = tagsNode[b].asString();
        return str::trim(value);
    };
    if (out->title.empty()) out->title = pick("title", "TITLE");
    if (out->artist.empty()) out->artist = pick("artist", "ARTIST");
    if (out->album.empty()) out->album = pick("album", "ALBUM");
    if (out->albumArtist.empty()) out->albumArtist = pick("album_artist", "ALBUM_ARTIST");
    if (out->genre.empty()) out->genre = pick("genre", "GENRE");
    if (out->year.empty()) out->year = pick("date", "DATE").substr(0, 4);
    if (out->trackNo == 0) out->trackNo = parseLeadingInt(pick("track", "TRACK"));
    if (out->parser.empty() || out->parser == "filename") out->parser = "ffprobe";
    return true;
}

bool TagReader::read(const std::string& path, Tags* out, std::string* error) {
    if (!out) return false;
    if (str::isUrl(path)) {
        out->title = str::fileName(path);
        out->parser = "url";
        readWithFfprobe(path, out);
        return true;
    }
    if (fileSizeOf(path) == 0) {
        if (error) *error = "file not found or empty: " + path;
        return false;
    }

    const std::string ext = str::extension(path);
    if (ext == "mp3") {
        const Bytes head = readFilePart(path, 1 << 20);
        parseId3v2(head, out, nullptr, nullptr);
        parseId3v1(path, out);
        estimateMp3Duration(path, head, out);
    } else if (ext == "flac") {
        parseFlac(path, out, nullptr, nullptr);
    } else if (ext == "wav" || ext == "aiff" || ext == "aif") {
        parseWavInfo(path, out);
    } else if (ext == "ogg" || ext == "oga" || ext == "opus") {
        parseOgg(path, out);
    } else if (ext == "m4a" || ext == "mp4" || ext == "aac" || ext == "alac") {
        const Bytes data = readFilePart(path, 1 << 21);
        walkMp4(data, 0, data.size(), out, nullptr, nullptr, 0);
    }

    // Fill the gaps with ffprobe when it is available.
    if (!out->complete() || out->sampleRate <= 0) {
        readWithFfprobe(path, out);
    }
    if (out->title.empty() || out->artist.empty()) {
        const Tags guessed = fromFileName(path);
        if (out->title.empty()) out->title = guessed.title;
        if (out->artist.empty()) out->artist = guessed.artist;
        if (out->trackNo == 0) out->trackNo = guessed.trackNo;
    }
    if (out->parser.empty()) out->parser = "filename";
    return true;
}

bool TagReader::readCover(const std::string& path, std::vector<unsigned char>* data,
                          std::string* mime) {
    if (!data) return false;
    data->clear();
    Tags tags;
    const std::string ext = str::extension(path);
    if (ext == "mp3") {
        const Bytes head = readFilePart(path, 1 << 22);
        parseId3v2(head, &tags, data, mime);
    } else if (ext == "flac") {
        parseFlac(path, &tags, data, mime);
    } else if (ext == "m4a" || ext == "mp4" || ext == "aac" || ext == "alac") {
        const Bytes file = readFilePart(path, 1 << 22);
        walkMp4(file, 0, file.size(), &tags, data, mime, 0);
    }
    if (!data->empty()) return true;

    const std::string sidecar = findSidecarCover(path);
    if (!sidecar.empty()) {
        std::ifstream in(sidecar, std::ios::binary);
        if (in) {
            data->assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
            if (mime) *mime = str::extension(sidecar) == "png" ? "image/png" : "image/jpeg";
            return !data->empty();
        }
    }
    return false;
}

std::string TagReader::findSidecarCover(const std::string& path) {
    static const char* kNames[] = {"cover.jpg", "cover.png", "folder.jpg", "front.jpg",
                                  "album.jpg", "albumart.jpg", "Cover.jpg", "Folder.jpg"};
    const std::string dir = str::parentDir(path);
    for (const char* name : kNames) {
        const std::string candidate = str::joinPath(dir, name);
        std::ifstream in(candidate, std::ios::binary);
        if (in) return candidate;
    }
    const std::string stemJpg = str::joinPath(dir, str::stem(path) + ".jpg");
    std::ifstream in(stemJpg, std::ios::binary);
    if (in) return stemJpg;
    return std::string();
}

std::string TagReader::findSidecarLyrics(const std::string& path) {
    const std::string dir = str::parentDir(path);
    const std::string base = str::stem(path);
    const char* extensions[] = {".lrc", ".LRC", ".txt"};
    for (const char* ext : extensions) {
        const std::string candidate = str::joinPath(dir, base + ext);
        std::ifstream in(candidate, std::ios::binary);
        if (in) return candidate;
    }
    return std::string();
}

} // namespace aurora
