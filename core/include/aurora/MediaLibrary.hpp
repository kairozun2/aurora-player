// Aurora Player - media library: incremental folder scan, JSON index, ranked search.
#pragma once

#include "aurora/TagReader.hpp"
#include "aurora/Types.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace aurora {

struct ScanReport {
    std::size_t filesSeen = 0;
    std::size_t added = 0;
    std::size_t updated = 0;
    std::size_t removed = 0;
    std::size_t skipped = 0;
    double elapsedSec = 0.0;
    bool cancelled = false;
};

struct AlbumInfo {
    std::string name;
    std::string artist;
    std::string year;
    std::size_t trackCount = 0;
    double durationSec = 0.0;
    std::string coverPath;
    std::vector<std::string> trackIds;
};

struct ArtistInfo {
    std::string name;
    std::size_t albumCount = 0;
    std::size_t trackCount = 0;
    double durationSec = 0.0;
};

struct LibraryStats {
    std::size_t trackCount = 0;
    std::size_t albumCount = 0;
    std::size_t artistCount = 0;
    std::size_t streamCount = 0;
    double totalDurationSec = 0.0;
    std::uint64_t totalBytes = 0;
};

class MediaLibrary {
public:
    using ProgressCallback = std::function<void(const std::string& path, std::size_t seen)>;

    explicit MediaLibrary(std::string indexPath = std::string());
    ~MediaLibrary();

    MediaLibrary(const MediaLibrary&) = delete;
    MediaLibrary& operator=(const MediaLibrary&) = delete;

    const std::string& indexPath() const { return indexPath_; }
    void setIndexPath(std::string path) { indexPath_ = std::move(path); }
    bool load();
    bool save() const;

    /// Recursive scan. `incremental` keeps entries whose size+mtime did not change.
    ScanReport scan(const std::vector<std::string>& roots, bool incremental = true);
    void scanAsync(const std::vector<std::string>& roots, bool incremental,
                   std::function<void(ScanReport)> onDone);
    void cancelScan() { cancel_.store(true); }
    bool scanning() const { return scanning_.load(); }
    void setProgressCallback(ProgressCallback callback);

    /// Adds one local file, folder entry or stream URL.
    bool addFile(const std::string& path, Track* out = nullptr);
    bool addStream(const std::string& url, const std::string& title, Track* out = nullptr);
    bool addDownloaded(const std::string& path, const std::string& sourceUrl, Track* out = nullptr);
    bool remove(const std::string& id);
    void clear();

    std::size_t size() const;
    std::vector<Track> tracks() const;
    bool track(const std::string& id, Track* out) const;
    bool trackByPath(const std::string& path, Track* out) const;

    /// Ranked fuzzy search over title/artist/album (RU + transliterated).
    std::vector<Track> search(const std::string& query, std::size_t limit = 60) const;
    std::vector<AlbumInfo> albums() const;
    std::vector<ArtistInfo> artists() const;
    std::vector<std::string> genres() const;
    std::vector<Track> byAlbum(const std::string& album) const;
    std::vector<Track> byArtist(const std::string& artist) const;
    std::vector<Track> byGenre(const std::string& genre) const;
    std::vector<Track> recentlyAdded(std::size_t limit = 25) const;
    std::vector<Track> recentlyPlayed(std::size_t limit = 25) const;
    std::vector<Track> mostPlayed(std::size_t limit = 25) const;
    std::vector<Track> favorites() const;
    std::vector<Track> neverPlayed(std::size_t limit = 25) const;
    std::vector<Track> shuffled(std::size_t limit = 50) const;

    void markPlayed(const std::string& id);
    bool setFavorite(const std::string& id, bool favorite);
    bool toggleFavorite(const std::string& id);
    bool setRating(const std::string& id, int rating);
    bool updateTrack(const Track& track);

    LibraryStats stats() const;
    std::vector<std::string> roots() const;
    void setRoots(std::vector<std::string> roots);
    void addRoot(const std::string& root);

    void setDecoderTools(std::string ffmpegPath, std::string ffprobePath);

private:
    void reindexUnlocked();
    bool readTrackUnlocked(const std::string& path, Track* out) const;
    static std::int64_t nowSeconds();

    std::string indexPath_;
    mutable std::mutex mutex_;
    std::vector<Track> tracks_;
    std::unordered_map<std::string, std::size_t> byId_;
    std::unordered_map<std::string, std::size_t> byPath_;
    std::vector<std::string> roots_;
    std::string ffmpegPath_ = "ffmpeg";
    std::string ffprobePath_ = "ffprobe";

    std::atomic<bool> cancel_{false};
    std::atomic<bool> scanning_{false};
    std::thread worker_;
    ProgressCallback progress_;
};

} // namespace aurora
