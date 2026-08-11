#include "PlayerBridge.hpp"

#include "CoverImageProvider.hpp"
#include "QtAudioSink.hpp"

#include "aurora/Analysis.hpp"
#include "aurora/Config.hpp"
#include "aurora/Decoder.hpp"
#include "aurora/Equalizer.hpp"
#include "aurora/I18n.hpp"
#include "aurora/Lyrics.hpp"
#include "aurora/Sink.hpp"
#include "aurora/Strings.hpp"
#include "aurora/Version.hpp"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QMetaObject>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <functional>
#include <thread>
#include <vector>

namespace aurora {
namespace {

QString qs(const std::string& value) { return QString::fromStdString(value); }

/// Translated core string, shared with the CLI catalogue (RU + EN).
QString ctr(const char* key) { return QString::fromStdString(aurora::tr(key)); }

QVariantMap toolEntry(const QString& name, bool available, const QString& detail) {
    QVariantMap map;
    map[QStringLiteral("name")] = name;
    map[QStringLiteral("available")] = available;
    map[QStringLiteral("detail")] = detail;
    return map;
}

} // namespace

PlayerBridge::PlayerBridge(QObject* parent) : QObject(parent) {}

PlayerBridge::~PlayerBridge() {
    poll_.stop();
    if (controller_.initialized()) {
        controller_.persist();
        controller_.shutdown();
    }
}

bool PlayerBridge::initialize(LibraryModel* library, AlbumModel* albums, QueueModel* queue,
                              DownloadsModel* downloads, QString* error) {
    library_ = library;
    albums_ = albums;
    queue_ = queue;
    downloads_ = downloads;

    std::string err;
    if (controller_.initialize(std::make_unique<QtAudioSink>(), &err)) {
        audioDevice_ = QtAudioSink::deviceDescription();
    } else {
        // No usable audio device (headless box, busy exclusive device, missing
        // drivers): keep the app alive in silent mode so the library, imports and
        // downloads still work, and tell the user why instead of just dying.
        const QString reason = qs(err);
        std::string silentError;
        if (!controller_.initialize(makeNullSink(1.0), &silentError)) {
            if (error) *error = reason.isEmpty() ? qs(silentError) : reason;
            return false;
        }
        audioDevice_.clear();
        QTimer::singleShot(1200, this, [this, reason] {
            emit errorOccurred(QObject::tr("Silent mode: no audio device available")
                               + (reason.isEmpty() ? QString()
                                                   : QStringLiteral(" \\u2014 ") + reason));
        });
    }

    // ---- core callbacks (fired on the controller pump thread) --------------
    Controller::Callbacks cb;
    cb.trackChanged = [this](const Track&) {
        toGui([this] {
            refreshTrackData();
            if (library_) library_->refresh();
            if (queue_) queue_->refresh();
            emit trackChanged();
            emit statusChanged();
        });
    };
    cb.stateChanged = [this](PlaybackState) {
        toGui([this] {
            emit stateChanged();
            emit statusChanged();
        });
    };
    cb.positionChanged = [this](double position, double duration) {
        toGui([this, position, duration] {
            position_ = position;
            if (duration > 0.0) duration_ = duration;
            emit positionChanged();
        });
    };
    cb.error = [this](const std::string& message) {
        toGui([this, message] { emit errorOccurred(qs(message)); });
    };
    cb.downloadChanged = [this](const DownloadJob& job) {
        const bool completed = job.state == DownloadState::Completed;
        const bool failed = job.state == DownloadState::Failed;
        const QString title = qs(job.title.empty() ? job.url : job.title);
        const QString reason = qs(job.error);
        toGui([this, completed, failed, title, reason] {
            if (downloads_) downloads_->refresh();
            if (completed) {
                if (library_) library_->refresh();
                if (albums_) albums_->refresh();
                emit libraryChanged();
                emit notice(ctr("download.ready") + QStringLiteral(": ") + title);
            } else if (failed) {
                emit errorOccurred(reason.isEmpty() ? title : title + QStringLiteral(" \\u2014 ") + reason);
            }
        });
    };
    cb.scanFinished = [this](const ScanReport& report) {
        const int added = static_cast<int>(report.added);
        const int updated = static_cast<int>(report.updated);
        toGui([this, added, updated] {
            if (library_) library_->refresh();
            if (albums_) albums_->refresh();
            emit libraryChanged();
            emit statusChanged();
            emit notice(QStringLiteral("%1: +%2 / ~%3").arg(ctr("library.title")).arg(added).arg(updated));
        });
    };
    cb.queueChanged = [this] {
        toGui([this] {
            if (queue_) queue_->refresh();
        });
    };
    controller_.setCallbacks(cb);

    // ---- models -----------------------------------------------------------
    if (library_) library_->setLibrary(&controller_.library());
    if (albums_) albums_->setLibrary(&controller_.library());
    if (queue_) queue_->setQueue(&controller_.queue());
    if (downloads_) downloads_->setDownloader(controller_.downloader());

    // ---- restore the previous session -------------------------------------
    const Settings& settings = controller_.config().settings();
    darkTheme_ = settings.theme != "light";

    if (settings.rememberPosition && !settings.lastTrackPath.empty()) {
        Track track;
        if (controller_.library().trackByPath(settings.lastTrackPath, &track)
            && controller_.playTrack(track)) {
            controller_.pause();
            if (settings.lastPositionSec > 1.0) controller_.seek(settings.lastPositionSec);
        }
    }
    if (settings.scanOnStart && !controller_.library().roots().empty()) {
        controller_.scanFoldersAsync();
    }

    refreshTrackData();

    poll_.setInterval(90);
    connect(&poll_, &QTimer::timeout, this, &PlayerBridge::pollEngine);
    poll_.start();

    emit themeChanged();
    emit languageChanged();
    emit settingsChanged();
    emit equalizerChanged();
    emit libraryChanged();
    emit statusChanged();
    return true;
}

void PlayerBridge::toGui(std::function<void()> fn) {
    QMetaObject::invokeMethod(this, std::move(fn), Qt::QueuedConnection);
}

void PlayerBridge::refreshTrackData() {
    const PlayerSnapshot snapshot = controller_.snapshot();
    position_ = snapshot.positionSec;
    duration_ = snapshot.durationSec;

    // Building a waveform decodes the whole file, so it must never run on the
    // window's thread. The result is dropped if the user already moved on.
    waveform_.clear();
    emit waveformChanged();
    const std::string wavePath = snapshot.track.path;
    if (!wavePath.empty()) {
        std::thread([this, wavePath] {
            Waveform wave;
            if (!controller_.waveformForCurrent(180, &wave) || !wave.valid()) return;
            const std::vector<float> peaks = wave.peaks;
            toGui([this, wavePath, peaks] {
                if (controller_.snapshot().track.path != wavePath) return;
                waveform_.clear();
                waveform_.reserve(static_cast<int>(peaks.size()));
                for (const float peak : peaks) waveform_.append(static_cast<double>(peak));
                emit waveformChanged();
            });
        }).detach();
    }

    lyrics_.clear();
    lyricTimes_.clear();
    const std::vector<LyricLine> lines = controller_.lyricLines();
    for (const LyricLine& line : lines) {
        lyrics_.append(qs(line.text));
        lyricTimes_.push_back(line.timeSec);
    }
    lyricsSynced_ = controller_.lyricsSynced();
    emit lyricsChanged();

    refreshPalette();
}

void PlayerBridge::refreshPalette() {
    const Track track = controller_.currentTrack();
    const std::string source = track.path.empty() ? track.sourceUrl : track.path;
    if (source.empty()) return;

    QImage cover;
    std::vector<unsigned char> bytes;
    std::string mime;
    if (TagReader::readCover(source, &bytes, &mime) && !bytes.empty()) {
        cover.loadFromData(reinterpret_cast<const uchar*>(bytes.data()),
                           static_cast<int>(bytes.size()));
    }
    if (cover.isNull()) cover = CoverImageProvider::placeholder(qs(source));

    QColor dominant = dominant_;
    QColor accent = accent_;
    CoverImageProvider::palette(cover, &dominant, &accent);
    if (dominant != dominant_ || accent != accent_) {
        dominant_ = dominant;
        accent_ = accent;
        emit paletteChanged();
    }
}

void PlayerBridge::pollEngine() {
    const PlayerSnapshot snapshot = controller_.snapshot();

    if (std::fabs(snapshot.positionSec - position_) > 0.03
        || std::fabs(snapshot.durationSec - duration_) > 0.03) {
        position_ = snapshot.positionSec;
        duration_ = snapshot.durationSec;
        emit positionChanged();
    }

    const bool nowPlaying = snapshot.state == PlaybackState::Playing;
    if (nowPlaying != wasPlaying_) {
        wasPlaying_ = nowPlaying;
        emit stateChanged();
        emit statusChanged();
    }

    float left = 0.0f;
    float right = 0.0f;
    controller_.engine().levels(&left, &right);
    // Smooth decay so the meters glide instead of flickering.
    const double smoothedLeft = std::max(static_cast<double>(left), levelLeft_ * 0.72);
    const double smoothedRight = std::max(static_cast<double>(right), levelRight_ * 0.72);
    if (std::fabs(smoothedLeft - levelLeft_) > 0.005
        || std::fabs(smoothedRight - levelRight_) > 0.005) {
        levelLeft_ = smoothedLeft;
        levelRight_ = smoothedRight;
        emit levelsChanged();
    }
}

QString PlayerBridge::localPath(const QString& maybeFileUrl) const {
    const QUrl url(maybeFileUrl);
    if (url.isLocalFile()) return url.toLocalFile();
    return maybeFileUrl;
}

// ------------------------------------------------------------- properties ---
QString PlayerBridge::title() const {
    const Track track = controller_.currentTrack();
    return track.valid() ? qs(track.displayTitle()) : QString();
}

QString PlayerBridge::artist() const {
    const Track track = controller_.currentTrack();
    return track.valid() ? qs(track.displayArtist()) : QString();
}

QString PlayerBridge::album() const { return qs(controller_.currentTrack().album); }

QString PlayerBridge::coverUrl() const {
    const Track track = controller_.currentTrack();
    return track.valid() ? coverUrlFor(track) : QString();
}

QString PlayerBridge::currentPath() const {
    const Track track = controller_.currentTrack();
    return qs(track.path.empty() ? track.sourceUrl : track.path);
}

bool PlayerBridge::isStream() const { return controller_.currentTrack().isStream(); }
bool PlayerBridge::favorite() const { return controller_.currentTrack().favorite; }

int PlayerBridge::currentLyricLine() const {
    if (!lyricsSynced_ || lyricTimes_.empty()) return -1;
    int result = -1;
    for (std::size_t i = 0; i < lyricTimes_.size(); ++i) {
        if (lyricTimes_[i] <= position_ + 0.15) result = static_cast<int>(i);
        else break;
    }
    return result;
}

bool PlayerBridge::playing() const {
    return controller_.snapshot().state == PlaybackState::Playing;
}

double PlayerBridge::volume() const { return controller_.snapshot().volume; }
bool PlayerBridge::muted() const { return controller_.snapshot().muted; }
int PlayerBridge::repeatMode() const { return static_cast<int>(controller_.snapshot().repeat); }
bool PlayerBridge::shuffle() const { return controller_.snapshot().shuffle; }
double PlayerBridge::speed() const { return controller_.snapshot().speed; }
double PlayerBridge::crossfade() const { return controller_.config().settings().crossfadeSec; }
bool PlayerBridge::gapless() const { return controller_.config().settings().gapless; }
bool PlayerBridge::rememberPosition() const {
    return controller_.config().settings().rememberPosition;
}

bool PlayerBridge::eqEnabled() const { return controller_.config().settings().equalizerEnabled; }
QString PlayerBridge::eqPreset() const {
    return qs(controller_.config().settings().equalizerPreset);
}

QStringList PlayerBridge::eqPresets() const {
    QStringList presets;
    for (const std::string& name : Equalizer::presetNames()) presets << qs(name);
    return presets;
}

double PlayerBridge::preampDb() const { return controller_.config().settings().preampDb; }
QString PlayerBridge::language() const { return qs(controller_.config().settings().language); }
QString PlayerBridge::statusLine() const { return qs(controller_.statusLine()); }

QString PlayerBridge::libraryStats() const {
    Controller& self = const_cast<PlayerBridge*>(this)->controller_;
    const LibraryStats stats = self.library().stats();
    return QStringLiteral("%1 \\u00b7 %2 \\u00b7 %3 \\u00b7 %4")
            .arg(ctr("library.tracks") + QStringLiteral(": %1").arg(stats.trackCount))
            .arg(ctr("library.albums") + QStringLiteral(": %1").arg(stats.albumCount))
            .arg(ctr("library.artists") + QStringLiteral(": %1").arg(stats.artistCount))
            .arg(formatDuration(stats.totalDurationSec));
}

QStringList PlayerBridge::musicFolders() const {
    Controller& self = const_cast<PlayerBridge*>(this)->controller_;
    QStringList folders;
    for (const std::string& root : self.library().roots()) folders << qs(root);
    return folders;
}

QVariantList PlayerBridge::toolStatus() const {
    Controller& self = const_cast<PlayerBridge*>(this)->controller_;
    const Settings& settings = controller_.config().settings();
    QVariantList list;

    const bool ffmpeg = hasFfmpeg(settings.ffmpegPath);
    list.append(toolEntry(QStringLiteral("ffmpeg"), ffmpeg,
                          ffmpeg ? QObject::tr("MP3, AAC, OGG, Opus, streams")
                                 : QObject::tr("WAV only until ffmpeg is installed")));

    Downloader* downloader = self.downloader();
    const bool ytdlp = downloader != nullptr && downloader->hasYtDlp();
    list.append(toolEntry(QStringLiteral("yt-dlp"), ytdlp,
                          ytdlp ? QObject::tr("YouTube and 1000+ sites")
                                : QObject::tr("Needed for YouTube links")));

    list.append(toolEntry(QObject::tr("Audio output"), !audioDevice_.isEmpty(),
                          audioDevice_.isEmpty() ? QObject::tr("No output device") : audioDevice_));
    return list;
}

QString PlayerBridge::aboutText() const {
    const Settings& settings = controller_.config().settings();
    return QObject::tr("Aurora Player %1 \\u00b7 %2 Hz \\u00b7 %3 ch\\nOutput: %4\\nConfig: %5")
            .arg(QStringLiteral(AURORA_VERSION_STRING))
            .arg(settings.sampleRate)
            .arg(settings.channels)
            .arg(audioDevice_.isEmpty() ? QObject::tr("none") : audioDevice_)
            .arg(qs(controller_.config().path()));
}

// -------------------------------------------------------------- transport ---
void PlayerBridge::play() {
    controller_.play();
    emit stateChanged();
}

void PlayerBridge::pause() {
    controller_.pause();
    emit stateChanged();
}

void PlayerBridge::togglePlayPause() {
    // Nothing loaded yet: start the library from the top so the button always works.
    if (!controller_.currentTrack().valid()) {
        if (library_ && library_->count() > 0) {
            controller_.playTracks(library_->rows(), 0);
            return;
        }
    }
    controller_.togglePlayPause();
    emit stateChanged();
}

void PlayerBridge::stop() {
    controller_.stop();
    emit stateChanged();
}

void PlayerBridge::next() { controller_.next(); }
void PlayerBridge::previous() { controller_.previous(); }

void PlayerBridge::seekFraction(double fraction) {
    if (!(duration_ > 0.0)) return;
    const double target = std::clamp(fraction, 0.0, 1.0) * duration_;
    controller_.seek(target);
    position_ = target;
    emit positionChanged();
}

void PlayerBridge::seekSeconds(double seconds) {
    const double limit = duration_ > 0.0 ? duration_ : seconds;
    const double target = std::clamp(seconds, 0.0, limit);
    controller_.seek(target);
    position_ = target;
    emit positionChanged();
}

void PlayerBridge::setVolume(double volume) {
    controller_.setVolume(static_cast<float>(std::clamp(volume, 0.0, 1.0)));
    emit settingsChanged();
}

void PlayerBridge::toggleMute() {
    controller_.toggleMute();
    emit settingsChanged();
}

void PlayerBridge::cycleRepeat() {
    controller_.cycleRepeat();
    emit settingsChanged();
}

void PlayerBridge::toggleShuffle() {
    controller_.toggleShuffle();
    if (queue_) queue_->refresh();
    emit settingsChanged();
}

void PlayerBridge::setCrossfade(double seconds) {
    controller_.setCrossfade(std::clamp(seconds, 0.0, 12.0));
    emit settingsChanged();
}

void PlayerBridge::setSpeed(double speed) {
    controller_.setSpeed(static_cast<float>(std::clamp(speed, 0.5, 2.0)));
    emit settingsChanged();
}

void PlayerBridge::setGapless(bool enabled) {
    controller_.setGapless(enabled);
    emit settingsChanged();
}

void PlayerBridge::setRememberPosition(bool enabled) {
    controller_.config().settings().rememberPosition = enabled;
    controller_.persist();
    emit settingsChanged();
}

// ---------------------------------------------------------- library / queue --
void PlayerBridge::playTrackAt(int index) {
    Track track;
    if (!library_ || !library_->at(index, &track)) return;
    // The visible list becomes the queue, exactly like clicking a row expects.
    std::string error;
    if (!controller_.playTracks(library_->rows(), index, &error) && !error.empty()) {
        emit errorOccurred(qs(error));
    }
}

void PlayerBridge::queueNext(int index) {
    Track track;
    if (!library_ || !library_->at(index, &track)) return;
    controller_.queue().insertNext(track);
    if (queue_) queue_->refresh();
    emit notice(QObject::tr("Play next") + QStringLiteral(": ") + qs(track.displayTitle()));
}

void PlayerBridge::toggleFavorite() {
    controller_.toggleFavoriteCurrent();
    if (library_) library_->refresh();
    emit trackChanged();
}

void PlayerBridge::toggleFavoriteAt(int index) {
    Track track;
    if (!library_ || !library_->at(index, &track)) return;
    controller_.library().toggleFavorite(track.id);
    library_->refresh();
    emit trackChanged();
    emit libraryChanged();
}

void PlayerBridge::playAlbum(int index) {
    AlbumInfo album;
    if (!albums_ || !albums_->at(index, &album)) return;
    const std::vector<Track> tracks = controller_.library().byAlbum(album.name);
    if (tracks.empty()) return;
    controller_.playTracks(tracks, 0);
}

void PlayerBridge::shuffleAll() {
    const std::vector<Track> tracks = controller_.library().shuffled(500);
    if (tracks.empty()) {
        emit notice(QObject::tr("Nothing here yet"));
        return;
    }
    controller_.setShuffle(true);
    controller_.playTracks(tracks, 0);
    emit settingsChanged();
}

void PlayerBridge::playQueueIndex(int index) {
    std::string error;
    if (!controller_.playQueueIndex(index, &error) && !error.empty()) emit errorOccurred(qs(error));
}

void PlayerBridge::removeFromQueue(int index) {
    if (index < 0) return;
    controller_.queue().removeAt(static_cast<std::size_t>(index));
    if (queue_) queue_->refresh();
}

void PlayerBridge::moveInQueue(int from, int to) {
    if (from < 0 || to < 0) return;
    controller_.queue().move(static_cast<std::size_t>(from), static_cast<std::size_t>(to));
    if (queue_) queue_->refresh();
}

void PlayerBridge::clearQueue() {
    controller_.queue().clear();
    if (queue_) queue_->refresh();
}

void PlayerBridge::scan() {
    if (controller_.library().roots().empty()) {
        emit notice(QObject::tr("Add a folder, a file or a link to get started"));
        return;
    }
    controller_.scanFoldersAsync();
    emit notice(QObject::tr("Rescan folders"));
}

void PlayerBridge::addPath(const QString& pathOrUrl) {
    const QString path = localPath(pathOrUrl).trimmed();
    if (path.isEmpty()) return;

    // Reading tags and walking a folder takes time, so it happens on its own
    // thread; the window stays responsive and reports the result when ready.
    const std::string input = path.toStdString();
    std::thread([this, input, path] {
        std::string message;
        const int added = controller_.add(input, false, &message);
        toGui([this, added, message, path] {
            if (library_) library_->refresh();
            if (albums_) albums_->refresh();
            if (downloads_) downloads_->refresh();
            emit libraryChanged();
            emit statusChanged();

            if (added > 0) {
                emit notice(QObject::tr("%1 tracks").arg(added));
            } else if (!message.empty()) {
                emit notice(qs(message));
            } else {
                emit errorOccurred(QObject::tr("Nothing to add") + QStringLiteral(": ") + path);
            }
        });
    }).detach();
}

void PlayerBridge::addFolder(const QString& folder) {
    const QString path = localPath(folder).trimmed();
    if (path.isEmpty()) return;

    const std::string root = QDir::toNativeSeparators(path).toStdString();
    Settings& settings = controller_.config().settings();
    if (std::find(settings.musicFolders.begin(), settings.musicFolders.end(), root)
        == settings.musicFolders.end()) {
        settings.musicFolders.push_back(root);
    }
    controller_.library().addRoot(root);
    controller_.persist();
    controller_.scanFoldersAsync();
    emit libraryChanged();
    emit notice(QObject::tr("Folder") + QStringLiteral(": ") + path);
}

void PlayerBridge::addUrl(const QString& url) {
    const QString trimmed = url.trimmed();
    if (trimmed.isEmpty()) return;

    // Same rule for links: the window must not wait for the network.
    const std::string input = trimmed.toStdString();
    std::thread([this, input] {
        std::string message;
        const int added = controller_.add(input, true, &message);
        toGui([this, added, message] {
            if (library_) library_->refresh();
            if (downloads_) downloads_->refresh();
            emit libraryChanged();
            if (!message.empty()) emit notice(qs(message));
            else if (added <= 0) emit notice(QObject::tr("Link / YouTube"));
        });
    }).detach();
}

void PlayerBridge::removeFolder(const QString& folder) {
    const std::string target = folder.toStdString();
    Settings& settings = controller_.config().settings();
    settings.musicFolders.erase(
            std::remove(settings.musicFolders.begin(), settings.musicFolders.end(), target),
            settings.musicFolders.end());
    controller_.library().setRoots(settings.musicFolders);
    controller_.persist();
    emit libraryChanged();
}

void PlayerBridge::cancelDownload(const QString& jobId) {
    if (Downloader* downloader = controller_.downloader()) {
        downloader->cancel(jobId.toStdString());
    }
    if (downloads_) downloads_->refresh();
}

void PlayerBridge::revealCurrent() {
    const Track track = controller_.currentTrack();
    if (!track.valid()) return;
    if (track.isStream() && !track.sourceUrl.empty()) {
        QDesktopServices::openUrl(QUrl(qs(track.sourceUrl)));
        return;
    }
    const QFileInfo info(qs(track.path));
    if (info.exists()) QDesktopServices::openUrl(QUrl::fromLocalFile(info.absolutePath()));
}

void PlayerBridge::seekToLyricLine(int line) {
    if (!lyricsSynced_ || line < 0 || line >= static_cast<int>(lyricTimes_.size())) return;
    seekSeconds(std::max(0.0, lyricTimes_[static_cast<std::size_t>(line)] - 0.12));
}

// ------------------------------------------------------- equaliser / shell ---
void PlayerBridge::setEqEnabled(bool enabled) {
    controller_.setEqualizerEnabled(enabled);
    emit equalizerChanged();
}

void PlayerBridge::applyPreset(const QString& preset) {
    controller_.applyEqualizerPreset(preset.toStdString());
    emit equalizerChanged();
}

double PlayerBridge::bandGain(int band) const {
    Controller& self = const_cast<PlayerBridge*>(this)->controller_;
    if (band < 0 || band >= Equalizer::kBands) return 0.0;
    return static_cast<double>(self.engine().equalizer().bandGain(band));
}

void PlayerBridge::setBand(int band, double db) {
    controller_.setEqualizerBand(band, static_cast<float>(std::clamp(db, -12.0, 12.0)));
    emit equalizerChanged();
}

void PlayerBridge::setPreamp(double db) {
    controller_.setPreampDb(static_cast<float>(std::clamp(db, -12.0, 12.0)));
    emit equalizerChanged();
}

void PlayerBridge::setLanguage(const QString& code) {
    const std::string wanted = code == QStringLiteral("ru") ? "ru" : "en";
    if (controller_.config().settings().language == wanted) return;
    controller_.setLanguage(wanted);
    emit languageChanged();
    emit statusChanged();
    emit libraryChanged();
}

void PlayerBridge::setTheme(const QString& theme) {
    const bool wantDark = theme != QStringLiteral("light");
    controller_.setTheme(wantDark ? "dark" : "light");
    if (darkTheme_ != wantDark) {
        darkTheme_ = wantDark;
        emit themeChanged();
    }
}

void PlayerBridge::setWindowSize(int width, int height) {
    if (width < 400 || height < 300) return;
    Settings& settings = controller_.config().settings();
    settings.windowWidth = width;
    settings.windowHeight = height;
}

} // namespace aurora
