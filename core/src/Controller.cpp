#include "aurora/Controller.hpp"

#include "aurora/I18n.hpp"
#include "aurora/Log.hpp"
#include "aurora/Strings.hpp"
#include "aurora/Version.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace fs = std::filesystem;

namespace aurora {

Controller::Controller() = default;

Controller::~Controller() { shutdown(); }

DecoderOptions Controller::decoderOptions() const {
    DecoderOptions options;
    options.sampleRate = config_.settings().sampleRate;
    options.channels = config_.settings().channels;
    options.ffmpegPath = config_.settings().ffmpegPath;
    options.ffprobePath = config_.settings().ffprobePath;
    return options;
}

bool Controller::initialize(std::unique_ptr<ISink> sink, std::string* error) {
    config_.load();
    Settings& settings = config_.settings();

    I18n::instance().setLanguage(settings.language);

    Config::ensureDir(Config::dataDir());
    Config::ensureDir(Config::cacheDir());
    Config::ensureDir(config_.coverCacheDir());
    Config::ensureDir(str::joinPath(Config::cacheDir(), "waveforms"));
    if (settings.downloadDir.empty()) settings.downloadDir = Config::defaultDownloadDir();
    Config::ensureDir(settings.downloadDir);

    library_.setIndexPath(config_.libraryIndexPath());
    library_.setDecoderTools(settings.ffmpegPath, settings.ffprobePath);
    library_.load();
    if (!settings.musicFolders.empty()) library_.setRoots(settings.musicFolders);

    playlists_.setPath(config_.playlistsPath());
    playlists_.load();

    downloader_.reset(new Downloader(settings.downloadDir, settings.ytdlpPath, settings.ffmpegPath));
    downloader_->setOnProgress([this](const DownloadJob& job) {
        Callbacks callbacks;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            callbacks = callbacks_;
        }
        if (callbacks.downloadChanged) callbacks.downloadChanged(job);
    });
    downloader_->setOnFinished([this](const DownloadJob& job) {
        if (job.state == DownloadState::Completed && !job.outputPath.empty()) {
            Track track;
            if (library_.addDownloaded(job.outputPath, job.url, &track)) {
                library_.save();
                bool autoPlay = false;
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    const auto found = std::find(autoPlayDownloads_.begin(),
                                                 autoPlayDownloads_.end(), job.id);
                    if (found != autoPlayDownloads_.end()) {
                        autoPlayDownloads_.erase(found);
                        autoPlay = true;
                    }
                }
                queue_.add(track);
                Callbacks callbacks;
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    callbacks = callbacks_;
                }
                if (callbacks.queueChanged) callbacks.queueChanged();
                if (autoPlay) playTrack(track, nullptr);
            }
        }
        Callbacks callbacks;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            callbacks = callbacks_;
        }
        if (callbacks.downloadChanged) callbacks.downloadChanged(job);
    });
    downloader_->start(2);

    engine_.setDecoderOptions(decoderOptions());
    if (!engine_.start(std::move(sink), error)) return false;
    wireEngine();
    applySettingsToEngine();

    {
        std::lock_guard<std::mutex> lock(eventMutex_);
        pumpRunning_ = true;
    }
    pump_ = std::thread([this] { pumpLoop(); });

    initialized_ = true;
    logInfo("controller", std::string("Aurora Player ") + versionString() + " ready (" +
                              std::to_string(library_.size()) + " tracks)");
    return true;
}

void Controller::shutdown() {
    if (!initialized_) return;
    initialized_ = false;
    persist();

    {
        std::lock_guard<std::mutex> lock(eventMutex_);
        pumpRunning_ = false;
    }
    eventCv_.notify_all();
    if (pump_.joinable()) pump_.join();

    if (downloader_) {
        downloader_->cancelAll();
        downloader_->stop();
    }
    engine_.shutdown();
}

void Controller::setCallbacks(Callbacks callbacks) {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_ = std::move(callbacks);
}

void Controller::wireEngine() {
    engine_.setNextUriProvider([this]() -> std::string {
        Track track;
        if (!queue_.peekNext(&track)) return std::string();
        return track.path;
    });
    engine_.onTrackAdvanced([this](const std::string& uri) {
        Event event;
        event.kind = EventKind::Advanced;
        event.uri = uri;
        post(event);
    });
    engine_.onFinished([this]() {
        Event event;
        event.kind = EventKind::Finished;
        post(event);
    });
    engine_.onStateChanged([this](PlaybackState state) {
        Callbacks callbacks;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            callbacks = callbacks_;
        }
        if (callbacks.stateChanged) callbacks.stateChanged(state);
    });
    engine_.onPositionChanged([this](double position, double duration) {
        Callbacks callbacks;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            callbacks = callbacks_;
        }
        if (callbacks.positionChanged) callbacks.positionChanged(position, duration);

        // Remember the resume position at most once every 5 seconds.
        if (config_.settings().rememberPosition &&
            std::fabs(position - lastPersistedPosition_) > 5.0) {
            lastPersistedPosition_ = position;
            config_.settings().lastPositionSec = position;
        }
    });
    engine_.onError([this](const std::string& message) {
        Callbacks callbacks;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            callbacks = callbacks_;
        }
        logError("engine", message);
        if (callbacks.error) callbacks.error(message);
    });
}

void Controller::applySettingsToEngine() {
    const Settings& settings = config_.settings();
    engine_.setVolume(settings.volume);
    engine_.setMuted(settings.muted);
    engine_.setSpeed(settings.speed);
    engine_.setCrossfadeSeconds(settings.crossfadeSec);
    engine_.setGapless(settings.gapless);
    engine_.equalizer().setEnabled(settings.equalizerEnabled);
    if (!settings.equalizerGains.empty()) {
        engine_.equalizer().setGains(settings.equalizerGains);
    } else if (!settings.equalizerPreset.empty()) {
        engine_.equalizer().applyPreset(settings.equalizerPreset);
    }
    engine_.equalizer().setPreampDb(settings.preampDb);
    queue_.setRepeat(settings.repeat);
    queue_.setShuffle(settings.shuffle);
}

void Controller::post(const Event& event) {
    {
        std::lock_guard<std::mutex> lock(eventMutex_);
        if (!pumpRunning_) return;
        events_.push_back(event);
    }
    eventCv_.notify_one();
}

void Controller::pumpLoop() {
    for (;;) {
        Event event;
        {
            std::unique_lock<std::mutex> lock(eventMutex_);
            eventCv_.wait(lock, [this] { return !events_.empty() || !pumpRunning_; });
            if (!pumpRunning_ && events_.empty()) return;
            event = events_.front();
            events_.pop_front();
        }
        switch (event.kind) {
            case EventKind::Advanced: handleAdvanced(event.uri); break;
            case EventKind::Finished: handleFinished(); break;
            case EventKind::Stop: break;
        }
    }
}

void Controller::handleAdvanced(const std::string& uri) {
    // The engine already switched to the preloaded stream: commit the queue.
    Track track;
    if (!queue_.next(&track, false)) {
        if (!library_.trackByPath(uri, &track)) {
            track = Track();
            track.path = uri;
            track.title = str::stem(uri);
        }
    }
    if (track.path != uri && !uri.empty()) {
        Track byPath;
        if (library_.trackByPath(uri, &byPath)) track = byPath;
    }
    setCurrentTrack(track);
}

void Controller::handleFinished() {
    Track track;
    if (queue_.next(&track, false)) {
        loadAndPlay(track, true, nullptr);
        return;
    }
    engine_.stop();
}

void Controller::setCurrentTrack(const Track& track) {
    Callbacks callbacks;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        current_ = track;
        callbacks = callbacks_;
    }
    if (!track.id.empty()) {
        library_.markPlayed(track.id);
        config_.settings().lastTrackPath = track.path;
    }
    loadLyricsFor(track);
    if (callbacks.trackChanged) callbacks.trackChanged(track);
}

void Controller::loadLyricsFor(const Track& track) {
    Lyrics lyrics;
    std::string path = track.lyricsPath;
    if (path.empty() && !track.path.empty() && !str::isUrl(track.path)) {
        path = TagReader::findSidecarLyrics(track.path);
    }
    if (!path.empty()) lyrics.loadFile(path);
    std::lock_guard<std::mutex> lock(mutex_);
    lyrics_ = lyrics;
}

bool Controller::loadAndPlay(const Track& track, bool autoPlay, std::string* error) {
    if (track.path.empty()) {
        if (error) *error = tr("error.notFound");
        return false;
    }
    std::string engineError;
    if (!engine_.load(track.path, autoPlay, &engineError)) {
        if (error) *error = engineError;
        Callbacks callbacks;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            callbacks = callbacks_;
        }
        if (callbacks.error) callbacks.error(engineError);
        return false;
    }
    setCurrentTrack(track);
    return true;
}

bool Controller::playTrack(const Track& track, std::string* error) {
    // Make sure the queue knows about this track so next/previous work.
    const std::vector<Track> tracks = queue_.tracks();
    int index = -1;
    for (std::size_t i = 0; i < tracks.size(); ++i) {
        if (tracks[i].path == track.path) {
            index = static_cast<int>(i);
            break;
        }
    }
    if (index < 0) {
        queue_.add(track);
        index = static_cast<int>(queue_.size()) - 1;
    }
    queue_.setCurrentIndex(index);
    return loadAndPlay(track, true, error);
}

bool Controller::playQueueIndex(int index, std::string* error) {
    Track track;
    if (!queue_.trackAt(static_cast<std::size_t>(index < 0 ? 0 : index), &track)) {
        if (error) *error = tr("queue.empty");
        return false;
    }
    queue_.setCurrentIndex(index);
    return loadAndPlay(track, true, error);
}

bool Controller::playTracks(const std::vector<Track>& tracks, int startIndex, std::string* error) {
    if (tracks.empty()) {
        if (error) *error = tr("library.empty");
        return false;
    }
    queue_.setTracks(tracks, startIndex);
    Callbacks callbacks;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callbacks = callbacks_;
    }
    if (callbacks.queueChanged) callbacks.queueChanged();
    Track track;
    if (!queue_.current(&track)) track = tracks[0];
    return loadAndPlay(track, true, error);
}

bool Controller::next() {
    Track track;
    if (!queue_.next(&track, true)) return false;
    return loadAndPlay(track, true, nullptr);
}

bool Controller::previous() {
    // Restart the track when more than 3 seconds have played (usual behaviour).
    if (engine_.position() > 3.0) return engine_.seek(0.0);
    Track track;
    if (!queue_.previous(&track)) return engine_.seek(0.0);
    return loadAndPlay(track, true, nullptr);
}

void Controller::play() { engine_.play(); }
void Controller::pause() { engine_.pause(); }
void Controller::togglePlayPause() { engine_.togglePlayPause(); }

void Controller::stop() {
    engine_.stop();
    config_.settings().lastPositionSec = 0.0;
}

bool Controller::seek(double seconds) { return engine_.seek(seconds); }
bool Controller::seekRelative(double deltaSeconds) { return engine_.seekRelative(deltaSeconds); }

void Controller::setVolume(float volume) {
    engine_.setVolume(volume);
    config_.settings().volume = engine_.volume();
}

float Controller::volume() const { return engine_.volume(); }

void Controller::setMuted(bool muted) {
    engine_.setMuted(muted);
    config_.settings().muted = muted;
}

void Controller::toggleMute() { setMuted(!engine_.muted()); }

void Controller::setSpeed(double speed) {
    engine_.setSpeed(speed);
    config_.settings().speed = engine_.speed();
}

void Controller::setRepeat(RepeatMode mode) {
    queue_.setRepeat(mode);
    config_.settings().repeat = mode;
}

RepeatMode Controller::repeat() const { return queue_.repeat(); }

void Controller::cycleRepeat() {
    switch (queue_.repeat()) {
        case RepeatMode::Off: setRepeat(RepeatMode::All); break;
        case RepeatMode::All: setRepeat(RepeatMode::One); break;
        case RepeatMode::One: setRepeat(RepeatMode::Off); break;
    }
}

void Controller::setShuffle(bool shuffle) {
    queue_.setShuffle(shuffle);
    config_.settings().shuffle = shuffle;
}

bool Controller::shuffle() const { return queue_.shuffle(); }
void Controller::toggleShuffle() { setShuffle(!queue_.shuffle()); }

void Controller::setCrossfade(double seconds) {
    engine_.setCrossfadeSeconds(seconds);
    config_.settings().crossfadeSec = engine_.crossfadeSeconds();
}

void Controller::setGapless(bool enabled) {
    engine_.setGapless(enabled);
    config_.settings().gapless = enabled;
}

void Controller::setEqualizerEnabled(bool enabled) {
    engine_.equalizer().setEnabled(enabled);
    config_.settings().equalizerEnabled = enabled;
}

bool Controller::applyEqualizerPreset(const std::string& preset) {
    if (!engine_.equalizer().applyPreset(preset)) return false;
    config_.settings().equalizerPreset = preset;
    config_.settings().equalizerGains = engine_.equalizer().gains();
    return true;
}

void Controller::setEqualizerBand(int band, float db) {
    engine_.equalizer().setBandGain(band, db);
    config_.settings().equalizerGains = engine_.equalizer().gains();
    config_.settings().equalizerPreset = "custom";
}

void Controller::setPreampDb(float db) {
    engine_.equalizer().setPreampDb(db);
    config_.settings().preampDb = engine_.equalizer().preampDb();
}

void Controller::setLanguage(const std::string& code) {
    if (I18n::instance().setLanguage(code)) config_.settings().language = code;
}

void Controller::setTheme(const std::string& theme) { config_.settings().theme = theme; }

int Controller::add(const std::string& pathOrUrl, bool playNow, std::string* message) {
    const std::string input = str::trim(pathOrUrl);
    if (input.empty()) return 0;

    // 1) Links -------------------------------------------------------------
    if (str::isUrl(input)) {
        if (Downloader::looksLikeDirectAudio(input)) {
            Track track;
            if (!library_.addStream(input, str::stem(input), &track)) return 0;
            library_.save();
            queue_.add(track);
            if (playNow) playTrack(track, nullptr);
            if (message) *message = tr("add.link") + ": " + track.title;
            return 1;
        }
        const std::string jobId = startDownload(input, playNow);
        if (message) {
            *message = jobId.empty() ? tr("download.needYtDlp") : tr("download.queued");
        }
        return jobId.empty() ? 0 : 1;
    }

    // 2) Playlists ---------------------------------------------------------
    if (str::toLower(str::extension(input)) == "m3u" || str::toLower(str::extension(input)) == "m3u8") {
        std::vector<std::string> paths;
        if (!queue_.loadM3u(input, &paths)) return 0;
        int added = 0;
        std::vector<Track> tracks;
        for (const std::string& path : paths) {
            Track track;
            if (library_.addFile(path, &track) || library_.trackByPath(path, &track)) {
                tracks.push_back(track);
                ++added;
            }
        }
        library_.save();
        if (!tracks.empty()) {
            if (playNow) playTracks(tracks, 0, nullptr);
            else queue_.addAll(tracks);
        }
        if (message) *message = tr("common.tracks", {std::to_string(added)});
        return added;
    }

    std::error_code ec;
    // 3) Folders -----------------------------------------------------------
    if (fs::is_directory(fs::path(input), ec)) {
        library_.addRoot(input);
        Settings& settings = config_.settings();
        if (std::find(settings.musicFolders.begin(), settings.musicFolders.end(), input) ==
            settings.musicFolders.end()) {
            settings.musicFolders.push_back(input);
        }
        const ScanReport report = library_.scan({input}, true);
        library_.save();
        config_.save();

        std::vector<Track> tracks;
        const std::string prefix = fs::weakly_canonical(fs::path(input), ec).string();
        for (const Track& track : library_.tracks()) {
            if (str::startsWith(track.path, prefix)) tracks.push_back(track);
        }
        std::sort(tracks.begin(), tracks.end(), [](const Track& a, const Track& b) {
            if (a.album != b.album) return a.album < b.album;
            if (a.discNo != b.discNo) return a.discNo < b.discNo;
            if (a.trackNo != b.trackNo) return a.trackNo < b.trackNo;
            return a.path < b.path;
        });
        if (!tracks.empty()) {
            if (playNow) playTracks(tracks, 0, nullptr);
            else queue_.addAll(tracks);
        }
        if (message) {
            *message = tr("library.scanDone") + ": +" + std::to_string(report.added) + " / " +
                       std::to_string(tracks.size());
        }
        return static_cast<int>(report.added);
    }

    // 4) Single file -------------------------------------------------------
    Track track;
    if (!library_.addFile(input, &track)) {
        if (message) *message = tr("error.notFound") + ": " + input;
        return 0;
    }
    library_.save();
    if (playNow) {
        playTrack(track, nullptr);
    } else {
        queue_.add(track);
    }
    if (message) *message = track.displayTitle();
    return 1;
}

ScanReport Controller::scanFolders(bool incremental) {
    const ScanReport report = library_.scan(config_.settings().musicFolders, incremental);
    library_.save();
    Callbacks callbacks;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callbacks = callbacks_;
    }
    if (callbacks.scanFinished) callbacks.scanFinished(report);
    return report;
}

void Controller::scanFoldersAsync() {
    library_.scanAsync(config_.settings().musicFolders, true, [this](ScanReport report) {
        Callbacks callbacks;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            callbacks = callbacks_;
        }
        if (callbacks.scanFinished) callbacks.scanFinished(report);
    });
}

std::string Controller::startDownload(const std::string& url, bool playWhenReady) {
    if (!downloader_) return std::string();
    const std::string id = downloader_->enqueue(url);
    if (!id.empty() && playWhenReady) {
        std::lock_guard<std::mutex> lock(mutex_);
        autoPlayDownloads_.push_back(id);
    }
    return id;
}

Track Controller::currentTrack() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_;
}

PlayerSnapshot Controller::snapshot() const {
    PlayerSnapshot snapshot = engine_.snapshot();
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot.track = current_;
    snapshot.repeat = queue_.repeat();
    snapshot.shuffle = queue_.shuffle();
    return snapshot;
}

std::vector<LyricLine> Controller::lyricLines() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lyrics_.lines();
}

bool Controller::lyricsSynced() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lyrics_.synced();
}

std::string Controller::lyricTextAt(double seconds) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lyrics_.textAt(seconds);
}

std::string Controller::waveformCachePath(const Track& track) const {
    const std::string id = track.id.empty() ? str::hashId(track.path) : track.id;
    return str::joinPath(str::joinPath(Config::cacheDir(), "waveforms"), id + ".json");
}

bool Controller::waveformForCurrent(int buckets, Waveform* out) {
    const Track track = currentTrack();
    if (track.path.empty() || !out) return false;
    const std::string cachePath = waveformCachePath(track);
    if (loadWaveform(cachePath, out) && static_cast<int>(out->peaks.size()) == buckets) return true;
    if (!computeWaveform(track.path, buckets, out, decoderOptions(), nullptr)) return false;
    saveWaveform(cachePath, *out);
    return true;
}

bool Controller::toggleFavoriteCurrent() {
    const Track track = currentTrack();
    if (track.id.empty()) return false;
    const bool favorite = library_.toggleFavorite(track.id);
    Track updated;
    if (library_.track(track.id, &updated)) {
        std::lock_guard<std::mutex> lock(mutex_);
        current_ = updated;
    }
    library_.save();
    return favorite;
}

std::string Controller::statusLine() const {
    const PlayerSnapshot snapshot = this->snapshot();
    std::string line;
    switch (snapshot.state) {
        case PlaybackState::Playing: line = "\u25b6 "; break;
        case PlaybackState::Paused: line = "\u23f8 "; break;
        case PlaybackState::Buffering: line = "\u2026 "; break;
        case PlaybackState::Error: line = "! "; break;
        case PlaybackState::Stopped: line = "\u25a0 "; break;
    }
    if (snapshot.track.path.empty()) {
        line += tr("queue.empty");
        return line;
    }
    std::string artist = snapshot.track.displayArtist();
    if (artist.empty()) artist = tr("common.unknownArtist");
    line += artist + " \u2014 " + snapshot.track.displayTitle();
    line += "  " + str::formatTime(snapshot.positionSec) + " / " +
            str::formatTime(snapshot.durationSec > 0 ? snapshot.durationSec
                                                     : snapshot.track.durationSec);
    line += "  " + tr("player.volume") + " " +
            std::to_string(static_cast<int>(snapshot.volume * 100.0f + 0.5f)) + "%";
    if (snapshot.shuffle) line += "  " + tr("player.shuffle");
    if (snapshot.repeat != RepeatMode::Off) {
        line += "  " + tr(snapshot.repeat == RepeatMode::All ? "player.repeat.all"
                                                            : "player.repeat.one");
    }
    if (engine_.equalizer().enabled()) {
        line += "  EQ:" + engine_.equalizer().currentPreset();
    }
    return line;
}

void Controller::persist() {
    Settings& settings = config_.settings();
    settings.repeat = queue_.repeat();
    settings.shuffle = queue_.shuffle();
    settings.volume = engine_.volume();
    settings.muted = engine_.muted();
    settings.speed = engine_.speed();
    settings.equalizerEnabled = engine_.equalizer().enabled();
    settings.equalizerGains = engine_.equalizer().gains();
    settings.preampDb = engine_.equalizer().preampDb();
    const Track track = currentTrack();
    if (!track.path.empty()) {
        settings.lastTrackPath = track.path;
        if (settings.rememberPosition) settings.lastPositionSec = engine_.position();
    }
    settings.musicFolders = library_.roots().empty() ? settings.musicFolders : library_.roots();
    config_.save();
    library_.save();
    playlists_.save();
}

} // namespace aurora
