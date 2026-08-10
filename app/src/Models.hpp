// Aurora Player - QAbstractListModel wrappers around the core data structures.
//
// The models never own data: they take snapshots from MediaLibrary / PlayQueue /
// Downloader and refresh on demand, which keeps the audio threads untouched by
// anything the GUI does.
#pragma once

#include "aurora/Downloader.hpp"
#include "aurora/MediaLibrary.hpp"
#include "aurora/Playlist.hpp"
#include "aurora/Types.hpp"

#include <QAbstractListModel>
#include <QHash>
#include <QString>

#include <vector>

namespace aurora {

/// Shared helper: "image://covers/<percent encoded path>".
QString coverUrlFor(const Track& track);
QString formatDuration(double seconds);

// ---------------------------------------------------------------------------
class LibraryModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        TitleRole = Qt::UserRole + 1,
        ArtistRole,
        AlbumRole,
        DurationTextRole,
        DurationRole,
        CoverUrlRole,
        FavoriteRole,
        PathRole,
        TrackIdRole,
        YearRole,
        GenreRole,
        PlayCountRole
    };

    explicit LibraryModel(QObject* parent = nullptr);

    void setLibrary(MediaLibrary* library);
    void refresh();

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(rows_.size()); }
    const std::vector<Track>& rows() const { return rows_; }
    bool at(int index, Track* out) const;

    Q_INVOKABLE void setFilter(const QString& text);
    Q_INVOKABLE void setFavouritesOnly(bool onlyFavourites);
    Q_INVOKABLE QString pathAt(int index) const;

signals:
    void countChanged();

private:
    MediaLibrary* library_ = nullptr;
    std::vector<Track> rows_;
    QString filter_;
    bool favouritesOnly_ = false;
};

// ---------------------------------------------------------------------------
class AlbumModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        ArtistRole,
        TitleRole,          // alias of NameRole, used by CoverFlow
        CoverUrlRole,
        TrackCountRole,
        YearRole,
        DurationTextRole
    };

    explicit AlbumModel(QObject* parent = nullptr);

    void setLibrary(MediaLibrary* library);
    void refresh();

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(rows_.size()); }
    bool at(int index, AlbumInfo* out) const;

signals:
    void countChanged();

private:
    MediaLibrary* library_ = nullptr;
    std::vector<AlbumInfo> rows_;
};

// ---------------------------------------------------------------------------
class QueueModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentIndexChanged)

public:
    enum Roles {
        TitleRole = Qt::UserRole + 1,
        ArtistRole,
        DurationTextRole,
        CoverUrlRole,
        IsCurrentRole,
        PathRole
    };

    explicit QueueModel(QObject* parent = nullptr);

    void setQueue(PlayQueue* queue);
    void refresh();

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(rows_.size()); }
    int currentIndex() const { return currentIndex_; }

signals:
    void countChanged();
    void currentIndexChanged();

private:
    PlayQueue* queue_ = nullptr;
    std::vector<Track> rows_;
    int currentIndex_ = -1;
};

// ---------------------------------------------------------------------------
class DownloadsModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int activeCount READ activeCount NOTIFY countChanged)

public:
    enum Roles {
        JobIdRole = Qt::UserRole + 1,
        TitleRole,
        UrlRole,
        StateTextRole,
        ProgressRole,
        DetailRole,
        FinishedRole,
        FailedRole
    };

    explicit DownloadsModel(QObject* parent = nullptr);

    void setDownloader(Downloader* downloader);
    void refresh();

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(rows_.size()); }
    int activeCount() const;

signals:
    void countChanged();

private:
    Downloader* downloader_ = nullptr;
    std::vector<DownloadJob> rows_;
};

} // namespace aurora
