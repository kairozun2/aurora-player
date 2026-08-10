#include "aurora/Strings.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace aurora {
namespace str {
namespace {

// Decode one UTF-8 code point; advances `i`. Returns 0xFFFD on malformed input.
unsigned int nextCodePoint(const std::string& s, std::size_t& i) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    if (c < 0x80) { ++i; return c; }
    if ((c >> 5) == 0x06 && i + 1 < s.size()) {
        const unsigned int cp = ((c & 0x1Fu) << 6) | (static_cast<unsigned char>(s[i + 1]) & 0x3Fu);
        i += 2;
        return cp;
    }
    if ((c >> 4) == 0x0E && i + 2 < s.size()) {
        const unsigned int cp = ((c & 0x0Fu) << 12) |
                                ((static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 6) |
                                (static_cast<unsigned char>(s[i + 2]) & 0x3Fu);
        i += 3;
        return cp;
    }
    if ((c >> 3) == 0x1E && i + 3 < s.size()) {
        const unsigned int cp = ((c & 0x07u) << 18) |
                                ((static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 12) |
                                ((static_cast<unsigned char>(s[i + 2]) & 0x3Fu) << 6) |
                                (static_cast<unsigned char>(s[i + 3]) & 0x3Fu);
        i += 4;
        return cp;
    }
    ++i;
    return 0xFFFDu;
}

void appendCodePoint(std::string& out, unsigned int cp) {
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

const char* cyrillicToLatin(unsigned int lowerCp) {
    switch (lowerCp) {
        case 0x0430: return "a";  case 0x0431: return "b";  case 0x0432: return "v";
        case 0x0433: return "g";  case 0x0434: return "d";  case 0x0435: return "e";
        case 0x0451: return "e";  case 0x0436: return "zh"; case 0x0437: return "z";
        case 0x0438: return "i";  case 0x0439: return "y";  case 0x043A: return "k";
        case 0x043B: return "l";  case 0x043C: return "m";  case 0x043D: return "n";
        case 0x043E: return "o";  case 0x043F: return "p";  case 0x0440: return "r";
        case 0x0441: return "s";  case 0x0442: return "t";  case 0x0443: return "u";
        case 0x0444: return "f";  case 0x0445: return "h";  case 0x0446: return "ts";
        case 0x0447: return "ch"; case 0x0448: return "sh"; case 0x0449: return "sch";
        case 0x044A: return "";   case 0x044B: return "y";  case 0x044C: return "";
        case 0x044D: return "e";  case 0x044E: return "yu"; case 0x044F: return "ya";
        default: return nullptr;
    }
}

} // namespace

std::string toLower(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    std::size_t i = 0;
    while (i < s.size()) {
        unsigned int cp = nextCodePoint(s, i);
        if (cp < 0x80) {
            out += static_cast<char>(std::tolower(static_cast<int>(cp)));
        } else if (cp >= 0x0410 && cp <= 0x042F) {   // А-Я
            appendCodePoint(out, cp + 0x20);
        } else if (cp == 0x0401) {                    // Ё
            appendCodePoint(out, 0x0451);
        } else if (cp >= 0x00C0 && cp <= 0x00DE && cp != 0x00D7) {
            appendCodePoint(out, cp + 0x20);
        } else {
            appendCodePoint(out, cp);
        }
    }
    return out;
}

std::string trim(const std::string& s) {
    std::size_t begin = 0;
    std::size_t end = s.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(s[begin]))) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(begin, end - begin);
}

std::vector<std::string> split(const std::string& s, char delim, bool keepEmpty) {
    std::vector<std::string> parts;
    std::string current;
    for (const char c : s) {
        if (c == delim) {
            if (keepEmpty || !current.empty()) parts.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    if (keepEmpty || !current.empty()) parts.push_back(current);
    return parts;
}

std::string join(const std::vector<std::string>& parts, const std::string& sep) {
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i) out += sep;
        out += parts[i];
    }
    return out;
}

bool startsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

bool endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool iContains(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    return toLower(haystack).find(toLower(needle)) != std::string::npos;
}

std::string replaceAll(std::string s, const std::string& from, const std::string& to) {
    if (from.empty()) return s;
    std::size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}

std::string normalize(const std::string& s) {
    const std::string lowered = toLower(s);
    std::string out;
    out.reserve(lowered.size());
    bool lastSpace = true;
    std::size_t i = 0;
    while (i < lowered.size()) {
        const std::size_t start = i;
        const unsigned int cp = nextCodePoint(lowered, i);
        const bool isAlnum = (cp < 0x80 && std::isalnum(static_cast<int>(cp))) || cp >= 0x80;
        if (isAlnum) {
            out.append(lowered, start, i - start);
            lastSpace = false;
        } else if (!lastSpace) {
            out += ' ';
            lastSpace = true;
        }
    }
    return trim(out);
}

std::string translit(const std::string& s) {
    const std::string lowered = toLower(s);
    std::string out;
    out.reserve(lowered.size());
    std::size_t i = 0;
    while (i < lowered.size()) {
        const std::size_t start = i;
        const unsigned int cp = nextCodePoint(lowered, i);
        if (const char* latin = cyrillicToLatin(cp)) {
            out += latin;
        } else {
            out.append(lowered, start, i - start);
        }
    }
    return out;
}

std::size_t utf8Length(const std::string& s) {
    std::size_t count = 0;
    std::size_t i = 0;
    while (i < s.size()) {
        nextCodePoint(s, i);
        ++count;
    }
    return count;
}

std::string ellipsize(const std::string& s, std::size_t maxChars) {
    if (maxChars == 0) return std::string();
    std::size_t i = 0;
    std::size_t count = 0;
    while (i < s.size()) {
        const std::size_t prev = i;
        nextCodePoint(s, i);
        if (++count > maxChars) {
            return s.substr(0, prev) + "\xE2\x80\xA6"; // ellipsis
        }
    }
    return s;
}

std::string formatTime(double seconds) {
    if (seconds < 0 || std::isnan(seconds)) seconds = 0;
    const long long total = static_cast<long long>(seconds + 0.0001);
    const long long h = total / 3600;
    const long long m = (total % 3600) / 60;
    const long long s = total % 60;
    char buf[32];
    if (h > 0) {
        std::snprintf(buf, sizeof(buf), "%lld:%02lld:%02lld", h, m, s);
    } else {
        std::snprintf(buf, sizeof(buf), "%lld:%02lld", m, s);
    }
    return std::string(buf);
}

std::string formatBytes(std::uint64_t bytes) {
    static const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        ++unit;
    }
    char buf[48];
    std::snprintf(buf, sizeof(buf), unit == 0 ? "%.0f %s" : "%.1f %s", value, units[unit]);
    return std::string(buf);
}

std::string formatDouble(double value, int decimals) {
    char fmt[16];
    std::snprintf(fmt, sizeof(fmt), "%%.%df", decimals < 0 ? 0 : decimals);
    char buf[64];
    std::snprintf(buf, sizeof(buf), fmt, value);
    return std::string(buf);
}

std::uint64_t hash64(const std::string& s) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (const unsigned char c : s) {
        hash ^= static_cast<std::uint64_t>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::string hashId(const std::string& s) {
    char buf[24];
    std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(hash64(s)));
    return std::string(buf);
}

std::string extension(const std::string& path) {
    const std::string name = fileName(path);
    const std::size_t dot = name.rfind('.');
    if (dot == std::string::npos || dot + 1 >= name.size()) return std::string();
    std::string ext = name.substr(dot + 1);
    const std::size_t query = ext.find_first_of("?#");
    if (query != std::string::npos) ext = ext.substr(0, query);
    return toLower(ext);
}

std::string fileName(const std::string& path) {
    const std::size_t pos = path.find_last_of("/\\");
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

std::string stem(const std::string& path) {
    const std::string name = fileName(path);
    const std::size_t dot = name.rfind('.');
    return dot == std::string::npos ? name : name.substr(0, dot);
}

std::string parentDir(const std::string& path) {
    const std::size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) return std::string(".");
    if (pos == 0) return std::string("/");
    return path.substr(0, pos);
}

std::string joinPath(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    const char last = a[a.size() - 1];
#ifdef _WIN32
    const char sep = '\\';
#else
    const char sep = '/';
#endif
    if (last == '/' || last == '\\') return a + b;
    return a + sep + b;
}

std::string sanitizeFileName(const std::string& name) {
    std::string out;
    out.reserve(name.size());
    for (const char c : name) {
        switch (c) {
            case '/': case '\\': case ':': case '*': case '?': case '"':
            case '<': case '>': case '|': case '\n': case '\r': case '\t':
                out += '_';
                break;
            default:
                out += c;
        }
    }
    out = trim(out);
    while (!out.empty() && (out[out.size() - 1] == '.' || out[out.size() - 1] == ' ')) {
        out.erase(out.size() - 1);
    }
    if (out.empty()) out = "track";
    if (out.size() > 150) out = out.substr(0, 150);
    return out;
}

bool isUrl(const std::string& s) {
    return s.find("://") != std::string::npos;
}

bool isHttpUrl(const std::string& s) {
    const std::string lowered = toLower(s);
    return startsWith(lowered, "http://") || startsWith(lowered, "https://");
}

std::string shellQuote(const std::string& s) {
#ifdef _WIN32
    std::string out = "\"";
    for (const char c : s) {
        if (c == '"') out += "\\\"";
        else out += c;
    }
    out += '"';
    return out;
#else
    std::string out = "'";
    for (const char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    out += '\'';
    return out;
#endif
}

} // namespace str
} // namespace aurora
