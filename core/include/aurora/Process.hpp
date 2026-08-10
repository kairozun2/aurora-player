// Aurora Player - minimal portable child-process helper (used for ffmpeg / yt-dlp).
#pragma once

#include <functional>
#include <string>
#include <vector>

namespace aurora {

class Process {
public:
    struct Result {
        int exitCode = -1;
        std::string output;   ///< stdout + stderr merged
        bool ok() const { return exitCode == 0; }
    };

    /// True when the executable can be resolved through PATH.
    static bool exists(const std::string& tool);

    /// Runs a command and captures its whole output.
    static Result run(const std::vector<std::string>& args);

    /// Runs a command and streams output line by line.
    /// Return false from `onLine` to abort early (used for cancelling downloads).
    static int runStreaming(const std::vector<std::string>& args,
                           const std::function<bool(const std::string&)>& onLine);

    /// Escapes and joins arguments into a shell command line.
    static std::string commandLine(const std::vector<std::string>& args);
};

} // namespace aurora
