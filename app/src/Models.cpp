#include "Models.hpp"

#include "aurora/Strings.hpp"

#include <QUrl>

#include <algorithm>
#include <cmath>

namespace aurora {

QString coverUrlFor(const Track& track) {
    // Streams have no file to read, so the placeholder is seeded with the URL.
    const std::string seed = track.path.empty() ? track.sourceUrl : track.path;
    if (seed.empty()) return QString();
    const QByteArray encoded = QUrl::toPercentEncoding(QString::fromStdString(seed));
    return QStringLiteral("image://covers/") + QString::fromUtf8(encoded);
}

QString formatDuration(double seconds) {
    if (!(seconds > 0.0)) return QStringLiteral("0:00");
    const long long total = static_cast<long long>(std::llround(seconds));
    const long long h = total / 3600;
    const long long m = (total % 3600) / 60;
    const long long s = total % 60;
    if (h > 0) {
        return QStringLiteral("%1:%2:%3")
                .arg(h)
                .arg(m, 2, 10, QLatin1Char('0'))
                .arg(s, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2").arg(m).arg(s, 2, 10, QLatin1Char('0'));
}

// ------------------------------------------------------------- LibraryModel --
LibraryModel::LibraryModel(QObject* parent) : QAbstractListModel(parent) {}

void LibraryModel::setLibrary(MediaLibrary* library) {
    library_ = library;
    refresh();
}

void LibraryModel::refresh() {
    beginResetModel();
    rows_.clear();
    if (library_) {
        if (!filter_.trimmed().isEmpty()) {
            rows_ = library_->search(filter_.trimmed().toStdString(), 500);
            if (favouritesOnly_) {
                rows_.erase(std::remove_if(rows_.begin(), rows_.end(),
                                           [](const Track& t) { return !t.favorite; }),
                            rows_.end());
            }
        } else if (favouritesOnly_) {
            rows_ = library_->favorites();
        } else {
            rows_ = library_->tracks();
            // Artist / album / track order reads far better than file order.
            std::sort(rows_.begin(), rows_.end(), [](const Track& a, const Track& b) {
                const std::string aa = str::toLower(a.displayArtist());
                const std::string ba = str::toLower(b.displayArtist());
                if (aa != ba) return aa < ba;
                if (a.album != b.album) return str::toLower(a.album) < str::toLower(b.album);
                if (a.discNo != b.discNo) return a.discNo < b.discNo;
                if (a.trackNo != b.trackNo) return a.trackNo < b.trackNo;
                return str::toLower(a.displayTitle()) < str::toLower(b.displayTitle());
            });
        }
    }
    endResetModel();
    emit countChanged();
}

int LibraryModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QVariant LibraryModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(rows_.size()))
        return QVariant();
    const Track& t = rows_[static_cast<std::size_t>(index.row())];
    switch (role) {
    case TitleRole: return QString::fromStdString(t.displayTitle());
    case ArtistRole: return QString::fromStdString(t.displayArtist());
    case AlbumRole: return QString::fromStdString(t.album);
    case DurationTextRole: return formatDuration(t.durationSec);
    case DurationRole: return t.durationSec;
    case CoverUrlRole: return coverUrlFor(t);
    case FavoriteRole: return t.favorite;
    case PathRole: return QString::fromStdString(t.path.empty() ? t.sourceUrl : t.path);
    case TrackIdRole: return QString::fromStdString(t.id);
    case YearRole: return QString::fromStdString(t.year);
    case GenreRole: return QString::fromStdString(t.genre);
    case PlayCountRole: return t.playCount;
    default: return QVariant();
    }
}

QHash<int, QByteArray> LibraryModel::roleNames() const {
    return {
        {TitleRole, "title"},
        {ArtistRole, "artist"},
        {AlbumRole, "album"},
        {DurationTextRole, "durationText"},
        {DurationRole, "duration"},
        {CoverUrlRole, "coverUrl"},
        {FavoriteRole, "favorite"},
        {PathRole, "path"},
        {TrackIdRole, "trackId"},
        {YearRole, "year"},
        {GenreRole, "genre"},
        {PlayCountRole, "playCount"},
    };
}

bool LibraryModel::at(int index, Track* out) const {
    if (index < 0 || index >= static_cast<int>(rows_.size()) || !out) return false;
    *out = rows_[static_cast<std::size_t>(index)];
    return true;
}

void LibraryModel::setFilter(const QString& text) {
    if (filter_ == text) return;
    filter_ = text;
    refresh();
}

void LibraryModel::setFavouritesOnly(bool onlyFavourites) {
    if (favouritesOnly_ == onlyFavourites) return;
    favouritesOnly_ = onlyFavourites;
    refresh();
}

QString LibraryModel::pathAt(int index) const {
    Track track;
    if (!at(index, &track)) return QString();
    return QString::fromStdString(track.path.empty() ? track.sourceUrl : track.path);
}

// --------------------------------------------------------------- AlbumModel --
AlbumModel::AlbumModel(QObject* parent) : QAbstractListModel(parent) {}

void AlbumModel::setLibrary(MediaLibrary* library) {
    library_ = library;
    refresh();
}

void AlbumModel::refresh() {
    beginResetModel();
    rows_ = library_ ? library_->albums() : std::vector<AlbumInfo>();
    endResetModel();
    emit countChanged();
}

int AlbumModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QVariant AlbumModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(rows_.size()))
        return QVariant();
    const AlbumInfo& album = rows_[static_cast<std::size_t>(index.row())];
    switch (role) {
    case NameRole:
    case TitleRole:
        return QString::fromStdString(album.name);
    case ArtistRole: return QString::fromStdString(album.artist);
    case CoverUrlRole: {
        if (!album.coverPath.empty()) {
            const QByteArray encoded =
                    QUrl::toPercentEncoding(QString::fromStdString(album.coverPath));
            return QStringLiteral("image://covers/") + QString::fromUtf8(encoded);
        }
        // Fall back to the first track of the album, which may hold embedded art.
        if (library_ && !album.trackIds.empty()) {
            Track track;
            if (library_->track(album.trackIds.front(), &track)) return coverUrlFor(track);
        }
        const QByteArray seed = QUrl::toPercentEncoding(
                QString::fromStdString(album.name + "|" + album.artist));
        return QStringLiteral("image://covers/") + QString::fromUtf8(seed);
    }
    case TrackCountRole: return static_cast<int>(album.trackCount);
    case YearRole: return QString::fromStdString(album.year);
    case DurationTextRole: return formatDuration(album.durationSec);
    default: return QVariant();
    }
}

QHash<int, QByteArray> AlbumModel::roleNames() const {
    return {
        {NameRole, "name"},
        {TitleRole, "title"},
        {ArtistRole, "artist"},
        {CoverUrlRole, "coverUrl"},
        {TrackCountRole, "trackCount"},
        {YearRole, "year"},
        {DurationTextRole, "durationText"},
    };
}

bool AlbumModel::at(int index, AlbumInfo* out) const {
    if (index < 0 || index >= static_cast<int>(rows_.size()) || !out) return false;
    *out = rows_[static_cast<std::size_t>(index)];
    return true;
}

// --------------------------------------------------------------- QueueModel --
QueueModel::QueueModel(QObject* parent) : QAbstractListModel(parent) {}

void QueueModel::setQueue(PlayQueue* queue) {
    queue_ = queue;
    refresh();
}

void QueueModel::refresh() {
    beginResetModel();
    rows_ = queue_ ? queue_->tracks() : std::vector<Track>();
    const int previous = currentIndex_;
    currentIndex_ = queue_ ? queue_->currentIndex() : -1;
    endResetModel();
    emit countChanged();
    if (previous != currentIndex_) emit currentIndexChanged();
}

int QueueModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QVariant QueueModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(rows_.size()))
        return QVariant();
    const Track& t = rows_[static_cast<std::size_t>(index.row())];
    switch (role) {
    case TitleRole: return QString::fromStdString(t.displayTitle());
    case ArtistRole: return QString::fromStdString(t.displayArtist());
    case DurationTextRole: return formatDuration(t.durationSec);
    case CoverUrlRole: return coverUrlFor(t);
    case IsCurrentRole: return index.row() == currentIndex_;
    case PathRole: return QString::fromStdString(t.path.empty() ? t.sourceUrl : t.path);
    default: return QVariant();
    }
}

QHash<int, QByteArray> QueueModel::roleNames() const {
    return {
        {TitleRole, "title"},
        {ArtistRole, "artist"},
        {DurationTextRole, "durationText"},
        {CoverUrlRole, "coverUrl"},
        {IsCurrentRole, "isCurrent"},
        {PathRole, "path"},
    };
}

// ----------------------------------------------------------- DownloadsModel --
DownloadsModel::DownloadsModel(QObject* parent) : QAbstractListModel(parent) {}

void DownloadsModel::setDownloader(Downloader* downloader) {
    downloader_ = downloader;
    refresh();
}

void DownloadsModel::refresh() {
    beginResetModel();
    rows_ = downloader_ ? downloader_->jobs() : std::vector<DownloadJob>();
    endResetModel();
    emit countChanged();
}

int DownloadsModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QVariant DownloadsModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(rows_.size()))
        return QVariant();
    const DownloadJob& job = rows_[static_cast<std::size_t>(index.row())];
    switch (role) {
    case JobIdRole: return QString::fromStdString(job.id);
    case TitleRole:
        return QString::fromStdString(job.title.empty() ? job.url : job.title);
    case UrlRole: return QString::fromStdString(job.url);
    case StateTextRole: {
        switch (job.state) {
        case DownloadState::Queued: return QObject::tr("Queued");
        case DownloadState::Running: return QObject::tr("Downloading");
        case DownloadState::Completed: return QObject::tr("Ready");
        case DownloadState::Failed: return QObject::tr("Failed");
        case DownloadState::Cancelled: return QObject::tr("Cancelled");
        }
        return QString();
    }
    case ProgressRole: return job.progress;
    case DetailRole: {
        if (job.state == DownloadState::Failed && !job.error.empty())
            return QString::fromStdString(job.error);
        QStringList parts;
        if (job.speedKbps > 0.0)
            parts << QStringLiteral("%1 KB/s").arg(job.speedKbps, 0, 'f', 0);
        if (job.etaSec > 0)
            parts << QObject::tr("%1 left").arg(formatDuration(job.etaSec));
        if (job.viaYtDlp) parts << QStringLiteral("yt-dlp");
        return parts.join(QStringLiteral("  \u00b7  "));
    }
    case FinishedRole: return job.state == DownloadState::Completed;
    case FailedRole:
        return job.state == DownloadState::Failed || job.state == DownloadState::Cancelled;
    default: return QVariant();
    }
}

QHash<int, QByteArray> DownloadsModel::roleNames() const {
    return {
        {JobIdRole, "jobId"},
        {TitleRole, "title"},
        {UrlRole, "url"},
        {StateTextRole, "stateText"},
        {ProgressRole, "progress"},
        {DetailRole, "detail"},
        {FinishedRole, "finished"},
        {FailedRole, "failed"},
    };
}

int DownloadsModel::activeCount() const {
    int active = 0;
    for (const DownloadJob& job : rows_) {
        if (job.state == DownloadState::Queued || job.state == DownloadState::Running) ++active;
    }
    return active;
}

} // namespace aurora
