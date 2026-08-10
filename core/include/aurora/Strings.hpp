// Aurora Player - UTF-8 aware string, path and formatting helpers.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace aurora {
namespace str {

/// Lowercase ASCII + Cyrillic (used for case-insensitive search in RU and EN).
std::string toLower(const std::string& s);
std::string trim(const std::string& s);
std::vector<std::string> split(const std::string& s, char delim, bool keepEmpty = false);
std::string join(const std::vector<std::string>& parts, const std::string& sep);
bool startsWith(const std::string& s, const std::string& prefix);
bool endsWith(const std::string& s, const std::string& suffix);
bool iContains(const std::string& haystack, const std::string& needle);
std::string replaceAll(std::string s, const std::string& from, const std::string& to);

/// Search normalisation: lowercase, punctuation stripped, whitespace collapsed.
std::string normalize(const std::string& s);
/// Cyrillic -> latin transliteration so "Кино" also matches "kino".
std::string translit(const std::string& s);
/// Number of UTF-8 code points (not bytes).
std::size_t utf8Length(const std::string& s);
/// Truncate to `maxChars` code points, appending an ellipsis when cut.
std::string ellipsize(const std::string& s, std::size_t maxChars);

// ---- formatting ----------------------------------------------------------
std::string formatTime(double seconds);       ///< 3:07 or 1:02:03
std::string formatBytes(std::uint64_t bytes); ///< 4.2 MB
std::string formatDouble(double value, int decimals);

// ---- ids ----------------------------------------------------------------
std::uint64_t hash64(const std::string& s);   ///< FNV-1a
std::string hashId(const std::string& s);     ///< 16 hex chars

// ---- paths --------------------------------------------------------------
std::string extension(const std::string& path);   ///< lowercase, without dot
std::string fileName(const std::string& path);
std::string stem(const std::string& path);
std::string parentDir(const std::string& path);
std::string joinPath(const std::string& a, const std::string& b);
std::string sanitizeFileName(const std::string& name);
bool isUrl(const std::string& s);
bool isHttpUrl(const std::string& s);
/// Quote an argument for a shell command line (POSIX single quotes / Windows quotes).
std::string shellQuote(const std::string& s);

} // namespace str
} // namespace aurora
