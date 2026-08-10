// Aurora Player - dependency-free JSON value, parser and serializer.
// Object keys keep insertion order so generated files are stable and diffable.
#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace aurora {

class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Json() = default;
    Json(bool v) : type_(Type::Bool), bool_(v) {}
    Json(double v) : type_(Type::Number), num_(v) {}
    Json(int v) : type_(Type::Number), num_(static_cast<double>(v)) {}
    Json(long long v) : type_(Type::Number), num_(static_cast<double>(v)) {}
    Json(const char* v) : type_(Type::String), str_(v ? v : "") {}
    Json(std::string v) : type_(Type::String), str_(std::move(v)) {}

    static Json object() { Json j; j.type_ = Type::Object; return j; }
    static Json array() { Json j; j.type_ = Type::Array; return j; }

    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::Null; }
    bool isBool() const { return type_ == Type::Bool; }
    bool isNumber() const { return type_ == Type::Number; }
    bool isString() const { return type_ == Type::String; }
    bool isArray() const { return type_ == Type::Array; }
    bool isObject() const { return type_ == Type::Object; }

    // ---- object -----------------------------------------------------------
    bool contains(const std::string& key) const { return find(key) != nullptr; }
    const Json* find(const std::string& key) const;
    Json& operator[](const std::string& key);              ///< creates the key if absent
    const Json& operator[](const std::string& key) const;  ///< null value if absent
    void set(const std::string& key, Json value) { (*this)[key] = std::move(value); }
    void erase(const std::string& key);
    const std::vector<std::pair<std::string, Json>>& items() const { return obj_; }

    // ---- array ------------------------------------------------------------
    std::size_t size() const;
    Json& at(std::size_t index);
    const Json& at(std::size_t index) const;
    void push(Json value);
    const std::vector<Json>& elements() const { return arr_; }

    // ---- scalars ----------------------------------------------------------
    std::string asString(const std::string& def = std::string()) const;
    double asDouble(double def = 0.0) const;
    long long asInt(long long def = 0) const;
    bool asBool(bool def = false) const;

    // ---- (de)serialization -------------------------------------------------
    std::string dump(int indent = 0) const;
    static Json parse(const std::string& text, std::string* error = nullptr);
    static Json parseFile(const std::string& path, std::string* error = nullptr);
    bool saveFile(const std::string& path, int indent = 2) const;

private:
    void dumpTo(std::string& out, int indent, int depth) const;

    Type type_ = Type::Null;
    bool bool_ = false;
    double num_ = 0.0;
    std::string str_;
    std::vector<Json> arr_;
    std::vector<std::pair<std::string, Json>> obj_;
};

} // namespace aurora
