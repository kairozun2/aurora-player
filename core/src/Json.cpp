#include "aurora/Json.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace aurora {
namespace {

const Json& nullValue() {
    static const Json value;
    return value;
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

void escapeTo(std::string& out, const std::string& in) {
    out += '"';
    for (unsigned char c : in) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    out += '"';
}

std::string numberToString(double v) {
    if (std::isnan(v) || std::isinf(v)) return "0";
    if (v == std::floor(v) && std::fabs(v) < 1e15) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(v));
        return std::string(buf);
    }
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%.10g", v);
    return std::string(buf);
}

class Parser {
public:
    explicit Parser(const std::string& text) : s_(text) {}

    bool parse(Json& out) {
        skipWs();
        if (!parseValue(out)) return false;
        skipWs();
        return true;
    }

    std::string error() const { return error_; }

private:
    bool fail(const std::string& msg) {
        if (error_.empty()) {
            error_ = msg + " at offset " + std::to_string(i_);
        }
        return false;
    }

    void skipWs() {
        while (i_ < s_.size()) {
            const char c = s_[i_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++i_;
            } else if (c == '/' && i_ + 1 < s_.size() && s_[i_ + 1] == '/') {
                while (i_ < s_.size() && s_[i_] != '\n') ++i_;
            } else {
                break;
            }
        }
    }

    bool parseValue(Json& out) {
        if (i_ >= s_.size()) return fail("unexpected end of input");
        const char c = s_[i_];
        switch (c) {
            case '{': return parseObject(out);
            case '[': return parseArray(out);
            case '"': {
                std::string str;
                if (!parseString(str)) return false;
                out = Json(str);
                return true;
            }
            case 't':
                if (s_.compare(i_, 4, "true") == 0) { i_ += 4; out = Json(true); return true; }
                return fail("invalid literal");
            case 'f':
                if (s_.compare(i_, 5, "false") == 0) { i_ += 5; out = Json(false); return true; }
                return fail("invalid literal");
            case 'n':
                if (s_.compare(i_, 4, "null") == 0) { i_ += 4; out = Json(); return true; }
                return fail("invalid literal");
            default: return parseNumber(out);
        }
    }

    bool parseNumber(Json& out) {
        const std::size_t start = i_;
        if (i_ < s_.size() && (s_[i_] == '-' || s_[i_] == '+')) ++i_;
        bool digits = false;
        while (i_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[i_]))) { ++i_; digits = true; }
        if (i_ < s_.size() && s_[i_] == '.') {
            ++i_;
            while (i_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[i_]))) { ++i_; digits = true; }
        }
        if (i_ < s_.size() && (s_[i_] == 'e' || s_[i_] == 'E')) {
            ++i_;
            if (i_ < s_.size() && (s_[i_] == '-' || s_[i_] == '+')) ++i_;
            while (i_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[i_]))) ++i_;
        }
        if (!digits) return fail("invalid number");
        out = Json(std::strtod(s_.substr(start, i_ - start).c_str(), nullptr));
        return true;
    }

    bool parseString(std::string& out) {
        if (s_[i_] != '"') return fail("expected string");
        ++i_;
        out.clear();
        while (i_ < s_.size()) {
            const char c = s_[i_++];
            if (c == '"') return true;
            if (c != '\\') { out += c; continue; }
            if (i_ >= s_.size()) break;
            const char esc = s_[i_++];
            switch (esc) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    if (i_ + 4 > s_.size()) return fail("bad \\u escape");
                    unsigned int cp = static_cast<unsigned int>(
                        std::strtoul(s_.substr(i_, 4).c_str(), nullptr, 16));
                    i_ += 4;
                    if (cp >= 0xD800 && cp <= 0xDBFF && i_ + 6 <= s_.size() &&
                        s_[i_] == '\\' && s_[i_ + 1] == 'u') {
                        const unsigned int low = static_cast<unsigned int>(
                            std::strtoul(s_.substr(i_ + 2, 4).c_str(), nullptr, 16));
                        if (low >= 0xDC00 && low <= 0xDFFF) {
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                            i_ += 6;
                        }
                    }
                    appendUtf8(out, cp);
                    break;
                }
                default: return fail("unknown escape");
            }
        }
        return fail("unterminated string");
    }

    bool parseArray(Json& out) {
        out = Json::array();
        ++i_; // '['
        skipWs();
        if (i_ < s_.size() && s_[i_] == ']') { ++i_; return true; }
        while (true) {
            skipWs();
            Json value;
            if (!parseValue(value)) return false;
            out.push(std::move(value));
            skipWs();
            if (i_ >= s_.size()) return fail("unterminated array");
            if (s_[i_] == ',') { ++i_; continue; }
            if (s_[i_] == ']') { ++i_; return true; }
            return fail("expected ',' or ']'");
        }
    }

    bool parseObject(Json& out) {
        out = Json::object();
        ++i_; // '{'
        skipWs();
        if (i_ < s_.size() && s_[i_] == '}') { ++i_; return true; }
        while (true) {
            skipWs();
            std::string key;
            if (i_ >= s_.size() || s_[i_] != '"') return fail("expected object key");
            if (!parseString(key)) return false;
            skipWs();
            if (i_ >= s_.size() || s_[i_] != ':') return fail("expected ':'");
            ++i_;
            skipWs();
            Json value;
            if (!parseValue(value)) return false;
            out[key] = std::move(value);
            skipWs();
            if (i_ >= s_.size()) return fail("unterminated object");
            if (s_[i_] == ',') { ++i_; continue; }
            if (s_[i_] == '}') { ++i_; return true; }
            return fail("expected ',' or '}'");
        }
    }

    const std::string& s_;
    std::size_t i_ = 0;
    std::string error_;
};

} // namespace

const Json* Json::find(const std::string& key) const {
    if (type_ != Type::Object) return nullptr;
    for (const auto& kv : obj_) {
        if (kv.first == key) return &kv.second;
    }
    return nullptr;
}

Json& Json::operator[](const std::string& key) {
    if (type_ != Type::Object) {
        type_ = Type::Object;
        arr_.clear();
    }
    for (auto& kv : obj_) {
        if (kv.first == key) return kv.second;
    }
    obj_.emplace_back(key, Json());
    return obj_.back().second;
}

const Json& Json::operator[](const std::string& key) const {
    const Json* found = find(key);
    return found ? *found : nullValue();
}

void Json::erase(const std::string& key) {
    for (std::size_t i = 0; i < obj_.size(); ++i) {
        if (obj_[i].first == key) {
            obj_.erase(obj_.begin() + static_cast<long>(i));
            return;
        }
    }
}

std::size_t Json::size() const {
    if (type_ == Type::Array) return arr_.size();
    if (type_ == Type::Object) return obj_.size();
    if (type_ == Type::String) return str_.size();
    return 0;
}

Json& Json::at(std::size_t index) {
    if (type_ != Type::Array) {
        type_ = Type::Array;
        obj_.clear();
    }
    if (index >= arr_.size()) arr_.resize(index + 1);
    return arr_[index];
}

const Json& Json::at(std::size_t index) const {
    if (type_ != Type::Array || index >= arr_.size()) return nullValue();
    return arr_[index];
}

void Json::push(Json value) {
    if (type_ != Type::Array) {
        type_ = Type::Array;
        obj_.clear();
    }
    arr_.push_back(std::move(value));
}

std::string Json::asString(const std::string& def) const {
    switch (type_) {
        case Type::String: return str_;
        case Type::Number: return numberToString(num_);
        case Type::Bool: return bool_ ? "true" : "false";
        default: return def;
    }
}

double Json::asDouble(double def) const {
    if (type_ == Type::Number) return num_;
    if (type_ == Type::Bool) return bool_ ? 1.0 : 0.0;
    if (type_ == Type::String) {
        try {
            return std::stod(str_);
        } catch (...) {
            return def;
        }
    }
    return def;
}

long long Json::asInt(long long def) const {
    if (type_ == Type::Number) return static_cast<long long>(num_ < 0 ? num_ - 0.5 : num_ + 0.5);
    if (type_ == Type::Bool) return bool_ ? 1 : 0;
    if (type_ == Type::String) {
        try {
            return std::stoll(str_);
        } catch (...) {
            return def;
        }
    }
    return def;
}

bool Json::asBool(bool def) const {
    if (type_ == Type::Bool) return bool_;
    if (type_ == Type::Number) return num_ != 0.0;
    if (type_ == Type::String) return str_ == "true" || str_ == "1" || str_ == "yes";
    return def;
}

void Json::dumpTo(std::string& out, int indent, int depth) const {
    const bool pretty = indent > 0;
    const std::string pad = pretty ? std::string(static_cast<std::size_t>(indent * (depth + 1)), ' ') : std::string();
    const std::string padEnd = pretty ? std::string(static_cast<std::size_t>(indent * depth), ' ') : std::string();
    switch (type_) {
        case Type::Null: out += "null"; break;
        case Type::Bool: out += bool_ ? "true" : "false"; break;
        case Type::Number: out += numberToString(num_); break;
        case Type::String: escapeTo(out, str_); break;
        case Type::Array: {
            if (arr_.empty()) { out += "[]"; break; }
            out += '[';
            for (std::size_t i = 0; i < arr_.size(); ++i) {
                if (i) out += ',';
                if (pretty) { out += '\n'; out += pad; }
                arr_[i].dumpTo(out, indent, depth + 1);
            }
            if (pretty) { out += '\n'; out += padEnd; }
            out += ']';
            break;
        }
        case Type::Object: {
            if (obj_.empty()) { out += "{}"; break; }
            out += '{';
            for (std::size_t i = 0; i < obj_.size(); ++i) {
                if (i) out += ',';
                if (pretty) { out += '\n'; out += pad; }
                escapeTo(out, obj_[i].first);
                out += pretty ? ": " : ":";
                obj_[i].second.dumpTo(out, indent, depth + 1);
            }
            if (pretty) { out += '\n'; out += padEnd; }
            out += '}';
            break;
        }
    }
}

std::string Json::dump(int indent) const {
    std::string out;
    out.reserve(256);
    dumpTo(out, indent, 0);
    return out;
}

Json Json::parse(const std::string& text, std::string* error) {
    Json result;
    Parser parser(text);
    if (!parser.parse(result)) {
        if (error) *error = parser.error();
        return Json();
    }
    if (error) error->clear();
    return result;
}

Json Json::parseFile(const std::string& path, std::string* error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (error) *error = "cannot open " + path;
        return Json();
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return parse(buffer.str(), error);
}

bool Json::saveFile(const std::string& path, int indent) const {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << dump(indent) << '\n';
    return out.good();
}

} // namespace aurora
