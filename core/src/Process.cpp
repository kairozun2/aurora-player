#include "aurora/Process.hpp"

#include "aurora/Strings.hpp"

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

#ifdef _WIN32
// A Qt GUI application owns no console. Anything started through popen() or
// system() therefore flashes a black command window on screen - and Aurora
// calls ffprobe once per scanned file, so the desktop fills with them. The
// Windows path below starts helpers with CREATE_NO_WINDOW and reads their
// output through a pipe, so nothing is ever visible.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#define AURORA_POPEN popen
#define AURORA_PCLOSE pclose
#endif

namespace aurora {
namespace {

#ifdef _WIN32

std::wstring widen(const std::string& text) {
    if (text.empty()) return std::wstring();
    const int needed = ::MultiByteToWideChar(CP_UTF8, 0, text.c_str(),
                                             static_cast<int>(text.size()), nullptr, 0);
    if (needed <= 0) return std::wstring();
    std::wstring wide(static_cast<std::size_t>(needed), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                          &wide[0], needed);
    return wide;
}

/// Runs `command` through cmd.exe with no window at all.
///
/// Raw bytes are appended to `rawOutput` when it is given; complete lines are
/// handed to `onLine` when it is given. Returning false from `onLine` stops the
/// child. Returns the exit code, -1 if the child could not be started, or 130
/// when the caller asked to stop.
int runHidden(const std::string& command,
              const std::function<bool(const std::string&)>& onLine,
              std::string* rawOutput) {
    SECURITY_ATTRIBUTES inheritable{};
    inheritable.nLength = sizeof(inheritable);
    inheritable.bInheritHandle = TRUE;
    inheritable.lpSecurityDescriptor = nullptr;

    HANDLE readEnd = nullptr;
    HANDLE writeEnd = nullptr;
    if (!::CreatePipe(&readEnd, &writeEnd, &inheritable, 0)) return -1;
    ::SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0);

    // Helpers must never wait on a console we do not have: hand them the null
    // device as their input.
    HANDLE nullInput = ::CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                     &inheritable, OPEN_EXISTING, 0, nullptr);

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    startup.hStdInput = (nullInput == INVALID_HANDLE_VALUE) ? nullptr : nullInput;
    startup.hStdOutput = writeEnd;
    startup.hStdError = writeEnd;

    // cmd.exe still does the quoting and the 2>&1 redirection, exactly like the
    // popen() version did, so callers keep working unchanged.
    const std::wstring full = L"cmd.exe /d /s /c \"" + widen(command) + L"\"";
    std::vector<wchar_t> buffer(full.begin(), full.end());
    buffer.push_back(L'\0');

    PROCESS_INFORMATION process{};
    const BOOL started = ::CreateProcessW(nullptr, buffer.data(), nullptr, nullptr, TRUE,
                                          CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);

    ::CloseHandle(writeEnd);
    if (nullInput != INVALID_HANDLE_VALUE) ::CloseHandle(nullInput);
    if (!started) {
        ::CloseHandle(readEnd);
        return -1;
    }

    std::string pending;
    bool aborted = false;
    char chunk[4096];
    DWORD got = 0;
    while (::ReadFile(readEnd, chunk, sizeof(chunk), &got, nullptr) && got > 0) {
        if (rawOutput) rawOutput->append(chunk, got);
        if (onLine) {
            for (DWORD i = 0; i < got; ++i) {
                const char ch = chunk[i];
                // yt-dlp reports progress with '\r', so both terminators end a line.
                if (ch == '\n' || ch == '\r') {
                    if (!pending.empty()) {
                        if (!onLine(pending)) { aborted = true; break; }
                        pending.clear();
                    }
                } else {
                    pending += ch;
                    if (pending.size() > 8192) pending.clear();
                }
            }
        }
        if (aborted) break;
    }
    if (!aborted && !pending.empty() && onLine) onLine(pending);

    if (aborted) ::TerminateProcess(process.hProcess, 1);
    ::WaitForSingleObject(process.hProcess, aborted ? 3000 : INFINITE);

    DWORD code = 0;
    if (!::GetExitCodeProcess(process.hProcess, &code)) code = 0;

    ::CloseHandle(process.hThread);
    ::CloseHandle(process.hProcess);
    ::CloseHandle(readEnd);

    if (aborted) return 130;
    return static_cast<int>(code);
}

#endif // _WIN32

} // namespace

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
    return runHidden("where " + str::shellQuote(tool), nullptr, nullptr) == 0;
#else
    const std::string cmd = "command -v " + str::shellQuote(tool) + " >/dev/null 2>&1";
    return std::system(cmd.c_str()) == 0;
#endif
}

Process::Result Process::run(const std::vector<std::string>& args) {
    Result result;
    if (args.empty()) return result;
    const std::string cmd = commandLine(args) + " 2>&1";
#ifdef _WIN32
    result.exitCode = runHidden(cmd, nullptr, &result.output);
#else
    std::FILE* pipe = AURORA_POPEN(cmd.c_str(), "r");
    if (!pipe) return result;
    char buffer[4096];
    while (std::fgets(buffer, sizeof(buffer), pipe)) {
        result.output += buffer;
    }
    const int status = AURORA_PCLOSE(pipe);
    result.exitCode = (status == -1) ? -1 : (status / 256);
#endif
    return result;
}

int Process::runStreaming(const std::vector<std::string>& args,
                          const std::function<bool(const std::string&)>& onLine) {
    if (args.empty()) return -1;
    const std::string cmd = commandLine(args) + " 2>&1";
#ifdef _WIN32
    return runHidden(cmd, onLine, nullptr);
#else
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
    return (status == -1) ? -1 : (status / 256);
#endif
}

} // namespace aurora
