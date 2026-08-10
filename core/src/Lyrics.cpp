#include "aurora/Lyrics.hpp"

#include "aurora/Strings.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace aurora {
namespace {

/// Parses "[mm:ss.xx]" / "[mm:ss]" and returns the seconds, or -1.
double parseTimestamp(const std::string& token) {
    const std::size_t colon = token.find(':');
    if (colon == std::string::npos) return -1.0;
    const std::string minutes = token.substr(0, colon);
    const std::string seconds = token.substr(colon + 1);
    if (minutes.empty() || seconds.empty()) return -1.0;
    for (const char c : minutes) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return -1.0;
    }
    char* end = nullptr;
    const double sec = std::strtod(seconds.c_str(), &end);
    if (end == seconds.c_str()) return -1.0;
    return std::strtod(minutes.c_str(), nullptr) * 60.0 + sec;
}

} // namespace

void Lyrics::clear() {
    lines_.clear();
    title_.clear();
    artist_.clear();
    offsetSec_ = 0.0;
    synced_ = false;
}

bool Lyrics::loadFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream buffer;
    buffer << in.rdbuf();
    loadText(buffer.str());
    return !lines_.empty();
}

void Lyrics::loadText(const std::string& text) {
    clear();
    std::istringstream stream(text);
    std::string rawLine;
    while (std::getline(stream, rawLine)) {
        if (!rawLine.empty() && rawLine[rawLine.size() - 1] == '\r') rawLine.erase(rawLine.size() - 1);
        std::string line = rawLine;

        std::vector<double> stamps;
        while (!line.empty() && line[0] == '[') {
            const std::size_t close = line.find(']');
            if (close == std::string::npos) break;
            const std::string token = line.substr(1, close - 1);
            const double seconds = parseTimestamp(token);
            if (seconds >= 0.0) {
                stamps.push_back(seconds);
            } else {
                // metadata tag: [ti:...] [ar:...] [offset:...]
                const std::size_t colon = token.find(':');
                if (colon != std::string::npos) {
                    const std::string key = str::toLower(str::trim(token.substr(0, colon)));
                    const std::string value = str::trim(token.substr(colon + 1));
                    if (key == "ti") title_ = value;
                    else if (key == "ar") artist_ = value;
                    else if (key == "offset") offsetSec_ = std::strtod(value.c_str(), nullptr) / 1000.0;
                }
            }
            line = line.substr(close + 1);
        }

        const std::string content = str::trim(line);
        if (stamps.empty()) {
            if (!content.empty()) {
                LyricLine entry;
                entry.timeSec = -1.0;
                entry.text = content;
                lines_.push_back(entry);
            }
            continue;
        }
        synced_ = true;
        for (const double seconds : stamps) {
            LyricLine entry;
            entry.timeSec = seconds;
            entry.text = content;
            lines_.push_back(entry);
        }
    }

    if (synced_) {
        std::stable_sort(lines_.begin(), lines_.end(),
                         [](const LyricLine& a, const LyricLine& b) { return a.timeSec < b.timeSec; });
    }
}

int Lyrics::indexAt(double seconds) const {
    if (!synced_ || lines_.empty()) return -1;
    const double target = seconds - offsetSec_;
    int index = -1;
    for (std::size_t i = 0; i < lines_.size(); ++i) {
        if (lines_[i].timeSec <= target) {
            index = static_cast<int>(i);
        } else {
            break;
        }
    }
    return index;
}

std::string Lyrics::textAt(double seconds) const {
    const int index = indexAt(seconds);
    if (index < 0) return std::string();
    return lines_[static_cast<std::size_t>(index)].text;
}

std::string Lyrics::plainText() const {
    std::string out;
    for (const LyricLine& line : lines_) {
        out += line.text;
        out += '\n';
    }
    return out;
}

} // namespace aurora
