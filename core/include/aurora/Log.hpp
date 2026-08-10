// Aurora Player - tiny thread-safe logger (no dependencies).
#pragma once

#include <string>

namespace aurora {

enum class LogLevel { Trace = 0, Debug = 1, Info = 2, Warn = 3, Error = 4, Off = 5 };

class Log {
public:
    static void setLevel(LogLevel level);
    static LogLevel level();
    /// Mirror output to a file (appends). Empty string disables the file sink.
    static void setFile(const std::string& path);
    static void setConsole(bool enabled);
    static void write(LogLevel level, const std::string& tag, const std::string& message);
};

void logTrace(const std::string& tag, const std::string& msg);
void logDebug(const std::string& tag, const std::string& msg);
void logInfo(const std::string& tag, const std::string& msg);
void logWarn(const std::string& tag, const std::string& msg);
void logError(const std::string& tag, const std::string& msg);

} // namespace aurora
