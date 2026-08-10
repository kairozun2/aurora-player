// Aurora Player - online import: YouTube (and 1000+ other sites) via yt-dlp,
// direct audio links via ffmpeg/curl. Runs in a small worker pool with
// progress reporting and cancellation.
#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace aurora {

enum class DownloadState { Queued, Running, Completed, Failed, Cancelled };

inline const char* toString(DownloadState state) {
    switch (state) {
        case DownloadState::Queued: return "queued";
        case DownloadState::Running: return "running";
        case DownloadState::Completed: return "completed";
        case DownloadState::Failed: return "failed";
        case DownloadState::Cancelled: return "cancelled";
    }
    return "unknown";
}

struct DownloadJob {
    std::string id;
    std::string url;
    std::string title;
    std::string outputPath;
    std::string error;
    DownloadState state = DownloadState::Queued;
    double progress = 0.0;     ///< 0..1
    double speedKbps = 0.0;
    int etaSec = -1;
    bool viaYtDlp = false;
};

class Downloader {
public:
    using JobCallback = std::function<void(const DownloadJob&)>;

    Downloader(std::string outputDir,
               std::string ytdlpPath = "yt-dlp",
               std::string ffmpegPath = "ffmpeg");
    ~Downloader();

    Downloader(const Downloader&) = delete;
    Downloader& operator=(const Downloader&) = delete;

    void start(int workers = 2);
    void stop();

    /// Queues a URL (YouTube page, playlist entry, direct mp3 link, ...).
    std::string enqueue(const std::string& url, const std::string& titleHint = std::string());
    bool cancel(const std::string& id);
    void cancelAll();

    std::vector<DownloadJob> jobs() const;
    bool job(const std::string& id, DownloadJob* out) const;
    std::size_t pending() const;
    /// Blocks until the queue is drained (used by the CLI).
    void waitAll();

    void setOnProgress(JobCallback callback);
    void setOnFinished(JobCallback callback);

    const std::string& outputDir() const { return outputDir_; }
    void setOutputDir(std::string dir);
    bool hasYtDlp() const;
    bool hasFfmpegTool() const;

    static bool isYouTubeUrl(const std::string& url);
    static bool looksLikeDirectAudio(const std::string& url);

private:
    void workerLoop();
    void runJob(DownloadJob job);
    bool runYtDlp(DownloadJob& job);
    bool runDirect(DownloadJob& job);
    void updateJob(const DownloadJob& job, bool finished);
    bool cancelled(const std::string& id) const;

    std::string outputDir_;
    std::string ytdlpPath_;
    std::string ffmpegPath_;

    mutable std::mutex mutex_;
    std::condition_variable queueCv_;
    std::condition_variable doneCv_;
    std::deque<std::string> queue_;
    std::vector<DownloadJob> jobs_;
    std::vector<std::string> cancelled_;
    std::vector<std::thread> workers_;
    std::atomic<bool> running_{false};
    std::atomic<int> active_{0};
    JobCallback onProgress_;
    JobCallback onFinished_;
};

} // namespace aurora
