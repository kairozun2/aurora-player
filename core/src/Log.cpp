#include "aurora/Log.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <mutex>

namespace aurora {
namespace {

std::atomic<int> g_level{static_cast<int>(LogLevel::Info)};
std::atomic<bool> g_console{true};
std::mutex g_mutex;
std::string g_file;

const char* levelName(LogLevel l) {
    switch (l) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info: return "INFO ";
        case LogLevel::Warn: return "WARN ";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Off: return "OFF  ";
    }
    return "?????";
}

std::string timestamp() {
    using clock = std::chrono::system_clock;
    const auto now = clock::now();
    const std::time_t t = clock::to_time_t(now);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now.time_since_epoch()) % std::chrono::seconds(1);
    std::tm tmv{};
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03d", tmv.tm_hour, tmv.tm_min,
                  tmv.tm_sec, static_cast<int>(ms.count()));
    return std::string(buf);
}

} // namespace

void Log::setLevel(LogLevel level) { g_level.store(static_cast<int>(level)); }
LogLevel Log::level() { return static_cast<LogLevel>(g_level.load()); }
void Log::setConsole(bool enabled) { g_console.store(enabled); }

void Log::setFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_file = path;
}

void Log::write(LogLevel level, const std::string& tag, const std::string& message) {
    if (static_cast<int>(level) < g_level.load()) return;
    const std::string line = timestamp() + " [" + levelName(level) + "] " + tag + ": " + message;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_console.load()) {
        std::fprintf(level >= LogLevel::Warn ? stderr : stdout, "%s\n", line.c_str());
    }
    if (!g_file.empty()) {
        if (std::FILE* f = std::fopen(g_file.c_str(), "a")) {
            std::fprintf(f, "%s\n", line.c_str());
            std::fclose(f);
        }
    }
}

void logTrace(const std::string& tag, const std::string& msg) { Log::write(LogLevel::Trace, tag, msg); }
void logDebug(const std::string& tag, const std::string& msg) { Log::write(LogLevel::Debug, tag, msg); }
void logInfo(const std::string& tag, const std::string& msg) { Log::write(LogLevel::Info, tag, msg); }
void logWarn(const std::string& tag, const std::string& msg) { Log::write(LogLevel::Warn, tag, msg); }
void logError(const std::string& tag, const std::string& msg) { Log::write(LogLevel::Error, tag, msg); }

} // namespace aurora
