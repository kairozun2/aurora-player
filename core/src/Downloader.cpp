#include "aurora/Downloader.hpp"

#include "aurora/Log.hpp"
#include "aurora/Process.hpp"
#include "aurora/Strings.hpp"

#include <algorithm>
#include <cstring>
#include <chrono>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

namespace aurora {
namespace {

/// Parses yt-dlp progress lines such as:
///   [download]  42.3% of 4.10MiB at 1.20MiB/s ETA 00:03
bool parseYtDlpProgress(const std::string& line, double* progress, double* speedKbps, int* etaSec) {
    const std::size_t percent = line.find('%');
    if (percent == std::string::npos) return false;
    std::size_t begin = percent;
    while (begin > 0 && (std::isdigit(static_cast<unsigned char>(line[begin - 1])) ||
                         line[begin - 1] == '.')) {
        --begin;
    }
    if (begin == percent) return false;
    *progress = std::strtod(line.substr(begin, percent - begin).c_str(), nullptr) / 100.0;

    const std::size_t at = line.find(" at ");
    if (at != std::string::npos) {
        const std::string speedText = str::trim(line.substr(at + 4));
        const double value = std::strtod(speedText.c_str(), nullptr);
        if (speedText.find("MiB") != std::string::npos) *speedKbps = value * 1024.0;
        else if (speedText.find("KiB") != std::string::npos) *speedKbps = value;
        else if (speedText.find("GiB") != std::string::npos) *speedKbps = value * 1024.0 * 1024.0;
    }
    const std::size_t eta = line.find("ETA ");
    if (eta != std::string::npos) {
        const std::string etaText = str::trim(line.substr(eta + 4));
        const std::vector<std::string> parts = str::split(etaText, ':');
        int seconds = 0;
        for (const std::string& part : parts) {
            seconds = seconds * 60 + static_cast<int>(std::strtol(part.c_str(), nullptr, 10));
        }
        *etaSec = seconds;
    }
    return true;
}

std::string extractDestination(const std::string& line) {
    static const char* kMarkers[] = {"[ExtractAudio] Destination: ", "[download] Destination: ",
                                    "[Merger] Merging formats into \""};
    for (const char* marker : kMarkers) {
        const std::size_t pos = line.find(marker);
        if (pos != std::string::npos) {
            std::string value = str::trim(line.substr(pos + std::strlen(marker)));
            if (!value.empty() && value[value.size() - 1] == '"') value.erase(value.size() - 1);
            return value;
        }
    }
    const std::size_t already = line.find("has already been downloaded");
    if (already != std::string::npos) {
        const std::size_t start = line.find("] ");
        if (start != std::string::npos && start + 2 < already) {
            return str::trim(line.substr(start + 2, already - start - 2));
        }
    }
    return std::string();
}

} // namespace

Downloader::Downloader(std::string outputDir, std::string ytdlpPath, std::string ffmpegPath)
    : outputDir_(std::move(outputDir)),
      ytdlpPath_(std::move(ytdlpPath)),
      ffmpegPath_(std::move(ffmpegPath)) {
    std::error_code ec;
    fs::create_directories(fs::path(outputDir_), ec);
}

Downloader::~Downloader() { stop(); }

void Downloader::start(int workers) {
    if (running_.exchange(true)) return;
    if (workers < 1) workers = 1;
    for (int i = 0; i < workers; ++i) {
        workers_.push_back(std::thread([this] { workerLoop(); }));
    }
    logInfo("download", "worker pool started (" + std::to_string(workers) + ")");
}

void Downloader::stop() {
    if (!running_.exchange(false)) return;
    queueCv_.notify_all();
    for (std::thread& worker : workers_) {
        if (worker.joinable()) worker.join();
    }
    workers_.clear();
}

void Downloader::setOutputDir(std::string dir) {
    std::error_code ec;
    fs::create_directories(fs::path(dir), ec);
    std::lock_guard<std::mutex> lock(mutex_);
    outputDir_ = std::move(dir);
}

void Downloader::setOnProgress(JobCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onProgress_ = std::move(callback);
}

void Downloader::setOnFinished(JobCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onFinished_ = std::move(callback);
}

bool Downloader::isYouTubeUrl(const std::string& url) {
    const std::string lower = str::toLower(url);
    return lower.find("youtube.com") != std::string::npos ||
           lower.find("youtu.be") != std::string::npos ||
           lower.find("music.youtube.com") != std::string::npos;
}

bool Downloader::looksLikeDirectAudio(const std::string& url) {
    const std::string lower = str::toLower(url);
    static const char* kExtensions[] = {".mp3", ".flac", ".wav", ".ogg", ".opus",
                                       ".m4a", ".aac",  ".wma", ".aiff"};
    for (const char* ext : kExtensions) {
        const std::size_t pos = lower.rfind(ext);
        if (pos != std::string::npos && pos + std::strlen(ext) >= lower.size() - 8) return true;
    }
    return false;
}

bool Downloader::hasYtDlp() const { return Process::exists(ytdlpPath_); }
bool Downloader::hasFfmpegTool() const { return Process::exists(ffmpegPath_); }

std::string Downloader::enqueue(const std::string& url, const std::string& titleHint) {
    if (url.empty()) return std::string();
    DownloadJob job;
    job.url = str::trim(url);
    job.title = titleHint.empty() ? job.url : titleHint;
    job.id = str::hashId(job.url + std::to_string(
                                       std::chrono::steady_clock::now().time_since_epoch().count()));
    job.viaYtDlp = !looksLikeDirectAudio(job.url);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        jobs_.push_back(job);
        queue_.push_back(job.id);
    }
    queueCv_.notify_one();
    logInfo("download", "queued " + job.url);
    return job.id;
}

bool Downloader::cancel(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (DownloadJob& job : jobs_) {
        if (job.id != id) continue;
        if (job.state == DownloadState::Completed) return false;
        cancelled_.push_back(id);
        if (job.state == DownloadState::Queued) {
            job.state = DownloadState::Cancelled;
            queue_.erase(std::remove(queue_.begin(), queue_.end(), id), queue_.end());
        }
        return true;
    }
    return false;
}

void Downloader::cancelAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (DownloadJob& job : jobs_) {
        if (job.state == DownloadState::Queued || job.state == DownloadState::Running) {
            cancelled_.push_back(job.id);
            if (job.state == DownloadState::Queued) job.state = DownloadState::Cancelled;
        }
    }
    queue_.clear();
    doneCv_.notify_all();
}

bool Downloader::cancelled(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::find(cancelled_.begin(), cancelled_.end(), id) != cancelled_.end();
}

std::vector<DownloadJob> Downloader::jobs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return jobs_;
}

bool Downloader::job(const std::string& id, DownloadJob* out) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const DownloadJob& job : jobs_) {
        if (job.id == id) {
            if (out) *out = job;
            return true;
        }
    }
    return false;
}

std::size_t Downloader::pending() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size() + static_cast<std::size_t>(active_.load());
}

void Downloader::waitAll() {
    std::unique_lock<std::mutex> lock(mutex_);
    doneCv_.wait(lock, [this] { return queue_.empty() && active_.load() == 0; });
}

void Downloader::updateJob(const DownloadJob& job, bool finished) {
    JobCallback progressCallback;
    JobCallback finishedCallback;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (DownloadJob& stored : jobs_) {
            if (stored.id == job.id) {
                stored = job;
                break;
            }
        }
        progressCallback = onProgress_;
        finishedCallback = onFinished_;
    }
    if (progressCallback) progressCallback(job);
    if (finished && finishedCallback) finishedCallback(job);
}

void Downloader::workerLoop() {
    while (running_.load()) {
        std::string id;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            queueCv_.wait_for(lock, std::chrono::milliseconds(200), [this] {
                return !queue_.empty() || !running_.load();
            });
            if (!running_.load()) return;
            if (queue_.empty()) continue;
            id = queue_.front();
            queue_.pop_front();
            active_.fetch_add(1);
        }

        DownloadJob job;
        if (this->job(id, &job)) runJob(job);

        active_.fetch_sub(1);
        doneCv_.notify_all();
    }
}

void Downloader::runJob(DownloadJob job) {
    if (cancelled(job.id)) {
        job.state = DownloadState::Cancelled;
        updateJob(job, true);
        return;
    }
    job.state = DownloadState::Running;
    job.progress = 0.0;
    updateJob(job, false);

    const bool ok = job.viaYtDlp ? runYtDlp(job) : runDirect(job);
    if (cancelled(job.id)) {
        job.state = DownloadState::Cancelled;
    } else if (ok && !job.outputPath.empty()) {
        job.state = DownloadState::Completed;
        job.progress = 1.0;
    } else {
        job.state = DownloadState::Failed;
        if (job.error.empty()) job.error = "download failed";
    }
    updateJob(job, true);
    logInfo("download", std::string(toString(job.state)) + ": " + job.url);
}

bool Downloader::runYtDlp(DownloadJob& job) {
    if (!hasYtDlp()) {
        job.error = "yt-dlp is not installed";
        return false;
    }
    std::string outputDir;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        outputDir = outputDir_;
    }
    std::error_code ec;
    fs::create_directories(fs::path(outputDir), ec);

    std::vector<std::string> args;
    args.push_back(ytdlpPath_);
    args.push_back("--no-playlist");
    args.push_back("--newline");
    args.push_back("--no-warnings");
    args.push_back("--extract-audio");
    args.push_back("--audio-format");
    args.push_back("mp3");
    args.push_back("--audio-quality");
    args.push_back("0");
    args.push_back("--embed-thumbnail");
    args.push_back("--embed-metadata");
    args.push_back("--add-metadata");
    if (hasFfmpegTool()) {
        args.push_back("--ffmpeg-location");
        args.push_back(ffmpegPath_);
    }
    args.push_back("-o");
    args.push_back(str::joinPath(outputDir, "%(title)s.%(ext)s"));
    args.push_back(job.url);

    std::string destination;
    std::string lastError;
    const int exitCode = Process::runStreaming(args, [&](const std::string& line) {
        if (cancelled(job.id)) return false;
        double progress = job.progress;
        double speed = job.speedKbps;
        int eta = job.etaSec;
        if (parseYtDlpProgress(line, &progress, &speed, &eta)) {
            job.progress = progress;
            job.speedKbps = speed;
            job.etaSec = eta;
            updateJob(job, false);
        }
        const std::string found = extractDestination(line);
        if (!found.empty()) destination = found;
        if (line.find("[youtube]") != std::string::npos && job.title == job.url) {
            // "[youtube] abc123: Downloading webpage" - keep the url as title for now
        }
        if (str::startsWith(line, "ERROR")) lastError = line;
        return true;
    });

    if (!lastError.empty()) job.error = lastError;
    if (exitCode != 0) {
        if (job.error.empty()) job.error = "yt-dlp exited with code " + std::to_string(exitCode);
        return false;
    }

    // Prefer the reported destination; fall back to the newest mp3 in the folder.
    if (!destination.empty()) {
        fs::path path(destination);
        if (path.extension() != ".mp3") path.replace_extension(".mp3");
        if (fs::exists(path, ec)) {
            job.outputPath = path.string();
            job.title = path.stem().string();
            return true;
        }
    }
    fs::file_time_type newest{};
    for (const fs::directory_entry& entry : fs::directory_iterator(outputDir, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        if (str::extension(entry.path().string()) != "mp3") continue;
        const auto time = fs::last_write_time(entry.path(), ec);
        if (ec) continue;
        if (job.outputPath.empty() || time > newest) {
            newest = time;
            job.outputPath = entry.path().string();
            job.title = entry.path().stem().string();
        }
    }
    return !job.outputPath.empty();
}

bool Downloader::runDirect(DownloadJob& job) {
    std::string outputDir;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        outputDir = outputDir_;
    }
    std::error_code ec;
    fs::create_directories(fs::path(outputDir), ec);

    std::string name = str::fileName(job.url);
    const std::size_t query = name.find('?');
    if (query != std::string::npos) name = name.substr(0, query);
    if (name.empty()) name = "download";
    name = str::sanitizeFileName(name);
    if (str::extension(name).empty()) name += ".mp3";
    const std::string output = str::joinPath(outputDir, name);

    std::vector<std::string> args;
    if (Process::exists("curl")) {
        args.push_back("curl");
        args.push_back("-L");
        args.push_back("--fail");
        args.push_back("--silent");
        args.push_back("--show-error");
        args.push_back("-o");
        args.push_back(output);
        args.push_back(job.url);
    } else if (hasFfmpegTool()) {
        args.push_back(ffmpegPath_);
        args.push_back("-y");
        args.push_back("-loglevel");
        args.push_back("error");
        args.push_back("-i");
        args.push_back(job.url);
        args.push_back("-c");
        args.push_back("copy");
        args.push_back(output);
    } else {
        job.error = "neither curl nor ffmpeg is available";
        return false;
    }

    const Process::Result result = Process::run(args);
    if (!result.ok()) {
        job.error = str::trim(result.output);
        if (job.error.empty()) job.error = "transfer failed";
        return false;
    }
    if (!fs::exists(fs::path(output), ec) || fs::file_size(fs::path(output), ec) == 0) {
        job.error = "empty download";
        return false;
    }
    job.outputPath = output;
    job.title = str::stem(output);
    job.progress = 1.0;
    return true;
}

} // namespace aurora
