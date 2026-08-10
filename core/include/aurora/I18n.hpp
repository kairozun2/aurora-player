// Aurora Player - localisation (Russian + English built in, JSON overrides).
#pragma once

#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace aurora {

class I18n {
public:
    static I18n& instance();

    /// "ru", "en" or "system" (detected from the environment).
    bool setLanguage(const std::string& code);
    const std::string& language() const;
    std::vector<std::string> availableLanguages() const;
    static std::string detectSystemLanguage();
    static std::string languageName(const std::string& code);

    /// Returns the translation for `key`, falling back to English, then the key.
    std::string tr(const std::string& key) const;
    /// Replaces {0}, {1}, ... with `args`.
    std::string tr(const std::string& key, const std::vector<std::string>& args) const;

    /// Loads i18n/<code>.json override files (flat key -> string maps).
    int loadDirectory(const std::string& dir);
    /// Adds or replaces a single string at runtime.
    void define(const std::string& language, const std::string& key, const std::string& value);

private:
    I18n();
    const std::map<std::string, std::string>* catalog(const std::string& code) const;

    mutable std::mutex mutex_;
    std::string language_ = "en";
    std::map<std::string, std::map<std::string, std::string>> catalogs_;
};

/// Shorthand used across the app.
std::string tr(const std::string& key);
std::string tr(const std::string& key, const std::vector<std::string>& args);

} // namespace aurora
