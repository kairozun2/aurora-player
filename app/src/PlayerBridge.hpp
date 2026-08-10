// Aurora Player - the single QObject that QML talks to.
//
// Everything the UI can do goes through here: playback, queue, library, imports,
// downloads, equaliser, language and theme. Core callbacks arrive on the
// controller's pump thread and are marshalled onto the GUI thread, so QML never
// sees data from a foreign thread.
#pragma once

#include "Models.hpp"

#include "aurora/Controller.hpp"

#include <QColor>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

#include <memory>

namespace aurora {

class PlayerBridge : public QObject {
    Q_OBJECT

    // Current track ---------------------------------------------------------
    Q_PROPERTY(QString title READ title NOTIFY trackChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY trackChanged)
    Q_PROPERTY(QString album READ album NOTIFY trackChanged)
    Q_PROPERTY(QString coverUrl READ coverUrl NOTIFY trackChanged)
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY trackChanged)
    Q_PROPERTY(bool isStream READ isStream NOTIFY trackChanged)
    Q_PROPERTY(bool favorite READ favorite NOTIFY trackChanged)
    Q_PROPERTY(QVariantList waveform READ waveform NOTIFY waveformChanged)
    Q_PROPERTY(QStringList lyrics READ lyrics NOTIFY lyricsChanged)
    Q_PROPERTY(bool lyricsSynced READ lyricsSynced NOTIFY lyricsChanged)
    Q_PROPERTY(int currentLyricLine READ currentLyricLine NOTIFY positionChanged)

    // Transport -------------------------------------------------------------
    Q_PROPERTY(bool playing READ playing NOTIFY stateChanged)
    Q_PROPERTY(double positionSec READ positionSec NOTIFY positionChanged)
    Q_PROPERTY(double durationSec READ durationSec NOTIFY positionChanged)
    Q_PROPERTY(QString positionText READ positionText NOTIFY positionChanged)
    Q_PROPERTY(QString durationText READ durationText NOTIFY positionChanged)
    Q_PROPERTY(double volume READ volume NOTIFY settingsChanged)
    Q_PROPERTY(bool muted READ muted NOTIFY settingsChanged)
    Q_PROPERTY(int repeatMode READ repeatMode NOTIFY settingsChanged)
    Q_PROPERTY(bool shuffle READ shuffle NOTIFY settingsChanged)
    Q_PROPERTY(double crossfade READ crossfade NOTIFY settingsChanged)
    Q_PROPERTY(double speed READ speed NOTIFY settingsChanged)
    Q_PROPERTY(bool gapless READ gapless NOTIFY settingsChanged)
    Q_PROPERTY(bool rememberPosition READ rememberPosition NOTIFY settingsChanged)
    Q_PROPERTY(double levelLeft READ levelLeft NOTIFY levelsChanged)
    Q_PROPERTY(double levelRight READ levelRight NOTIFY levelsChanged)

    // Equaliser -------------------------------------------------------------
    Q_PROPERTY(bool eqEnabled READ eqEnabled NOTIFY equalizerChanged)
    Q_PROPERTY(QString eqPreset READ eqPreset NOTIFY equalizerChanged)
    Q_PROPERTY(QStringList eqPresets READ eqPresets CONSTANT)
    Q_PROPERTY(double preampDb READ preampDb NOTIFY equalizerChanged)

    // Shell -----------------------------------------------------------------
    Q_PROPERTY(QString language READ language NOTIFY languageChanged)
    Q_PROPERTY(bool darkTheme READ darkTheme NOTIFY themeChanged)
    Q_PROPERTY(QColor dominantColor READ dominantColor NOTIFY paletteChanged)
    Q_PROPERTY(QColor accentColor READ accentColor NOTIFY paletteChanged)
    Q_PROPERTY(QString statusLine READ statusLine NOTIFY statusChanged)
    Q_PROPERTY(QString libraryStats READ libraryStats NOTIFY libraryChanged)
    Q_PROPERTY(QString aboutText READ aboutText NOTIFY statusChanged)
    Q_PROPERTY(QStringList musicFolders READ musicFolders NOTIFY libraryChanged)
    Q_PROPERTY(QVariantList toolStatus READ toolStatus NOTIFY statusChanged)

public:
    explicit PlayerBridge(QObject* parent = nullptr);
    ~PlayerBridge() override;

    /// Starts the core, restores settings and wires callbacks. Returns false and
    /// fills `error` when audio output cannot be opened at all.
    bool initialize(LibraryModel* library, AlbumModel* albums, QueueModel* queue,
                    DownloadsModel* downloads, QString* error);

    // Property getters ------------------------------------------------------
    QString title() const;
    QString artist() const;
    QString album() const;
    QString coverUrl() const;
    QString currentPath() const;
    bool isStream() const;
    bool favorite() const;
    QVariantList waveform() const { return waveform_; }
    QStringList lyrics() const { return lyrics_; }
    bool lyricsSynced() const { return lyricsSynced_; }
    int currentLyricLine() const;

    bool playing() const;
    double positionSec() const { return position_; }
    double durationSec() const { return duration_; }
    QString positionText() const { return formatDuration(position_); }
    QString durationText() const { return formatDuration(duration_); }
    double volume() const;
    bool muted() const;
    int repeatMode() const;
    bool shuffle() const;
    double crossfade() const;
    double speed() const;
    bool gapless() const;
    bool rememberPosition() const;
    double levelLeft() const { return levelLeft_; }
    double levelRight() const { return levelRight_; }

    bool eqEnabled() const;
    QString eqPreset() const;
    QStringList eqPresets() const;
    double preampDb() const;

    QString language() const;
    bool darkTheme() const { return darkTheme_; }
    QColor dominantColor() const { return dominant_; }
    QColor accentColor() const { return accent_; }
    QString statusLine() const;
    QString libraryStats() const;
    QString aboutText() const;
    QStringList musicFolders() const;
    QVariantList toolStatus() const;

    // Transport -------------------------------------------------------------
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void togglePlayPause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void seekFraction(double fraction);
    Q_INVOKABLE void seekSeconds(double seconds);
    Q_INVOKABLE void setVolume(double volume);
    Q_INVOKABLE void toggleMute();
    Q_INVOKABLE void cycleRepeat();
    Q_INVOKABLE void toggleShuffle();
    Q_INVOKABLE void setCrossfade(double seconds);
    Q_INVOKABLE void setSpeed(double speed);
    Q_INVOKABLE void setGapless(bool enabled);
    Q_INVOKABLE void setRememberPosition(bool enabled);

    // Library / queue -------------------------------------------------------
    Q_INVOKABLE void playTrackAt(int index);
    Q_INVOKABLE void queueNext(int index);
    Q_INVOKABLE void toggleFavorite();
    Q_INVOKABLE void toggleFavoriteAt(int index);
    Q_INVOKABLE void playAlbum(int index);
    Q_INVOKABLE void shuffleAll();
    Q_INVOKABLE void playQueueIndex(int index);
    Q_INVOKABLE void removeFromQueue(int index);
    Q_INVOKABLE void moveInQueue(int from, int to);
    Q_INVOKABLE void clearQueue();
    Q_INVOKABLE void scan();
    Q_INVOKABLE void addPath(const QString& pathOrUrl);
    Q_INVOKABLE void addFolder(const QString& folder);
    Q_INVOKABLE void addUrl(const QString& url);
    Q_INVOKABLE void removeFolder(const QString& folder);
    Q_INVOKABLE void cancelDownload(const QString& jobId);
    Q_INVOKABLE void revealCurrent();
    Q_INVOKABLE void seekToLyricLine(int line);

    // Equaliser / shell -----------------------------------------------------
    Q_INVOKABLE void setEqEnabled(bool enabled);
    Q_INVOKABLE void applyPreset(const QString& preset);
    Q_INVOKABLE double bandGain(int band) const;
    Q_INVOKABLE void setBand(int band, double db);
    Q_INVOKABLE void setPreamp(double db);
    Q_INVOKABLE void setLanguage(const QString& code);
    Q_INVOKABLE void setTheme(const QString& theme);
    Q_INVOKABLE void setWindowSize(int width, int height);

signals:
    void trackChanged();
    void stateChanged();
    void positionChanged();
    void settingsChanged();
    void equalizerChanged();
    void languageChanged();
    void themeChanged();
    void paletteChanged();
    void statusChanged();
    void libraryChanged();
    void levelsChanged();
    void waveformChanged();
    void lyricsChanged();
    void notice(const QString& message);
    void errorOccurred(const QString& message);

private:
    /// Runs `fn` on the GUI thread, whatever thread called it.
    void toGui(std::function<void()> fn);
    void refreshTrackData();
    void refreshPalette();
    void pollEngine();
    QString localPath(const QString& maybeFileUrl) const;

    Controller controller_;
    LibraryModel* library_ = nullptr;
    AlbumModel* albums_ = nullptr;
    QueueModel* queue_ = nullptr;
    DownloadsModel* downloads_ = nullptr;

    QTimer poll_;
    double position_ = 0.0;
    double duration_ = 0.0;
    double levelLeft_ = 0.0;
    double levelRight_ = 0.0;
    bool darkTheme_ = true;
    bool wasPlaying_ = false;
    QColor dominant_ = QColor("#2A1D16");
    QColor accent_ = QColor("#F0A45E");
    QVariantList waveform_;
    QStringList lyrics_;
    std::vector<double> lyricTimes_;
    bool lyricsSynced_ = false;
    QString audioDevice_;
};

} // namespace aurora
