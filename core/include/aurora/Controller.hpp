// Aurora Player - application brain shared by the Qt UI and the CLI.
//
// It owns the engine, the library, the queue, playlists, downloads and the
// settings, and it implements the rules that tie them together (auto-advance,
// gapless preload, play counts, resume position, ...).
//
// Threading: engine callbacks arrive on the decode thread, so they are turned
// into events and processed on a dedicated controller thread. That way loading
// the next track never happens inside the audio/decode path.
#pragma once

#include "aurora/Analysis.hpp"
#include "aurora/AudioEngine.hpp"
#include "aurora/Config.hpp"
#include "aurora/Downloader.hpp"
#include "aurora/Lyrics.hpp"
#include "aurora/MediaLibrary.hpp"
#include "aurora/Playlist.hpp"
#include "aurora/Types.hpp"

#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace aurora {

class Controller {
public:
    struct Callbacks {
        std::function<void(const Track&)> trackChanged;
        std::function<void(PlaybackState)> stateChanged;
        std::function<void(double positionSec, double durationSec)> positionChanged;
        std::function<void(const std::string& message)> error;
        std::function<void(const DownloadJob&)> downloadChanged;
        std::function<void(const ScanReport&)> scanFinished;
        std::function<void()> queueChanged;
    };

    Controller();
    ~Controller();

    Controller(const Controller&) = delete;
    Controller& operator=(const Controller&) = delete;

    /// Loads settings + library, starts the engine on `sink`.
    bool initialize(std::unique_ptr<ISink> sink, std::string* error);
    void shutdown();
    bool initialized() const { return initialized_; }

    Config& config() { return config_; }
    const Config& config() const { return config_; }
    MediaLibrary& library() { return library_; }
    PlayQueue& queue() { return queue_; }
    PlaylistStore& playlists() { return playlists_; }
    AudioEngine& engine() { return engine_; }
    Downloader* downloader() { return downloader_.get(); }

    void setCallbacks(Callbacks callbacks);

    // ---- content ---------------------------------------------------------
    /// Accepts a file, a folder, an .m3u playlist, a direct audio link, or a
    /// YouTube/video link (queued in the downloader). Returns tracks added.
    int add(const std::string& pathOrUrl, bool playNow, std::string* message = nullptr);
    ScanReport scanFolders(bool incremental = true);
    void scanFoldersAsync();
    std::string startDownload(const std::string& url, bool playWhenReady = false);

    // ---- playback --------------------------------------------------------
    bool playTrack(const Track& track, std::string* error = nullptr);
    bool playQueueIndex(int index, std::string* error = nullptr);
    bool playTracks(const std::vector<Track>& tracks, int startIndex = 0, std::string* error = nullptr);
    bool next();
    bool previous();
    void play();
    void pause();
    void togglePlayPause();
    void stop();
    bool seek(double seconds);
    bool seekRelative(double deltaSeconds);

    void setVolume(float volume);
    float volume() const;
    void setMuted(bool muted);
    void toggleMute();
    void setSpeed(double speed);

    void setRepeat(RepeatMode mode);
    RepeatMode repeat() const;
    void cycleRepeat();
    void setShuffle(bool shuffle);
    bool shuffle() const;
    void toggleShuffle();
    void setCrossfade(double seconds);
    void setGapless(bool enabled);

    void setEqualizerEnabled(bool enabled);
    bool applyEqualizerPreset(const std::string& preset);
    void setEqualizerBand(int band, float db);
    void setPreampDb(float db);

    void setLanguage(const std::string& code);
    void setTheme(const std::string& theme);

    // ---- state -----------------------------------------------------------
    Track currentTrack() const;
    PlayerSnapshot snapshot() const;
    std::string statusLine() const;
    std::vector<LyricLine> lyricLines() const;
    bool lyricsSynced() const;
    std::string lyricTextAt(double seconds) const;
    /// Cached waveform for the current track (computed on first use).
    bool waveformForCurrent(int buckets, Waveform* out);
    bool toggleFavoriteCurrent();

    /// Persists settings, library index and playlists.
    void persist();

private:
    enum class EventKind { Finished, Advanced, Stop };
    struct Event {
        EventKind kind = EventKind::Stop;
        std::string uri;
    };

    void wireEngine();
    void applySettingsToEngine();
    bool loadAndPlay(const Track& track, bool autoPlay, std::string* error);
    void setCurrentTrack(const Track& track);
    void post(const Event& event);
    void pumpLoop();
    void handleFinished();
    void handleAdvanced(const std::string& uri);
    void loadLyricsFor(const Track& track);
    std::string waveformCachePath(const Track& track) const;
    DecoderOptions decoderOptions() const;

    Config config_;
    MediaLibrary library_;
    PlayQueue queue_;
    PlaylistStore playlists_;
    AudioEngine engine_;
    std::unique_ptr<Downloader> downloader_;

    mutable std::mutex mutex_;
    Track current_;
    Lyrics lyrics_;
    Callbacks callbacks_;
    bool initialized_ = false;
    double lastPersistedPosition_ = 0.0;
    std::vector<std::string> autoPlayDownloads_;

    std::thread pump_;
    std::mutex eventMutex_;
    std::condition_variable eventCv_;
    std::deque<Event> events_;
    bool pumpRunning_ = false;
};

} // namespace aurora
