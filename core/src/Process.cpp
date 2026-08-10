#include "aurora/Process.hpp"

#include "aurora/Strings.hpp"

#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#define AURORA_POPEN _popen
#define AURORA_PCLOSE _pclose
#else
#define AURORA_POPEN popen
#define AURORA_PCLOSE pclose
#endif

namespace aurora {

std::string Process::commandLine(const std::vector<std::string>& args) {
    std::string cmd;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (i) cmd += ' ';
        cmd += str::shellQuote(args[i]);
    }
    return cmd;
}

bool Process::exists(const std::string& tool) {
    if (tool.empty()) return false;
#ifdef _WIN32
    const std::string cmd = "where " + str::shellQuote(tool) + " >NUL 2>NUL";
#else
    const std::string cmd = "command -v " + str::shellQuote(tool) + " >/dev/null 2>&1";
#endif
    return std::system(cmd.c_str()) == 0;
}

Process::Result Process::run(const std::vector<std::string>& args) {
    Result result;
    if (args.empty()) return result;
    const std::string cmd = commandLine(args) + " 2>&1";
    std::FILE* pipe = AURORA_POPEN(cmd.c_str(), "r");
    if (!pipe) return result;
    char buffer[4096];
    while (std::fgets(buffer, sizeof(buffer), pipe)) {
        result.output += buffer;
    }
    const int status = AURORA_PCLOSE(pipe);
#ifdef _WIN32
    result.exitCode = status;
#else
    result.exitCode = (status == -1) ? -1 : (status / 256);
#endif
    return result;
}

int Process::runStreaming(const std::vector<std::string>& args,
                          const std::function<bool(const std::string&)>& onLine) {
    if (args.empty()) return -1;
    const std::string cmd = commandLine(args) + " 2>&1";
    std::FILE* pipe = AURORA_POPEN(cmd.c_str(), "r");
    if (!pipe) return -1;

    std::string line;
    bool aborted = false;
    int ch = 0;
    while ((ch = std::fgetc(pipe)) != EOF) {
        // yt-dlp reports progress with '\r', so treat both terminators as line ends.
        if (ch == '\n' || ch == '\r') {
            if (!line.empty()) {
                if (onLine && !onLine(line)) { aborted = true; break; }
                line.clear();
            }
        } else {
            line += static_cast<char>(ch);
            if (line.size() > 8192) line.clear();
        }
    }
    if (!aborted && !line.empty() && onLine) onLine(line);

    const int status = AURORA_PCLOSE(pipe);
    if (aborted) return 130; // treated as "cancelled"
#ifdef _WIN32
    return status;
#else
    return (status == -1) ? -1 : (status / 256);
#endif
}

} // namespace aurora
