#include "aurora/MediaLibrary.hpp"

#include "aurora/Decoder.hpp"
#include "aurora/Json.hpp"
#include "aurora/Log.hpp"
#include "aurora/Strings.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <random>
#include <set>

namespace fs = std::filesystem;

namespace aurora {
namespace {

constexpr int kIndexVersion = 1;

std::string canonicalPath(const std::string& path) {
    if (str::isUrl(path)) return path;
    std::error_code ec;
    const fs::path resolved = fs::weakly_canonical(fs::path(path), ec);
    if (ec) return path;
    return resolved.string();
}

Json trackToJson(const Track& track) {
    Json json = Json::object();
    json.set("id", track.id);
    json.set("path", track.path);
    json.set("title", track.title);
    json.set("artist", track.artist);
    json.set("album", track.album);
    json.set("albumArtist", track.albumArtist);
    json.set("genre", track.genre);
    json.set("year", track.year);
    json.set("trackNo", track.trackNo);
    json.set("discNo", track.discNo);
    json.set("duration", track.durationSec);
    json.set("sampleRate", track.sampleRate);
    json.set("channels", track.channels);
    json.set("bitrate", track.bitrateKbps);
    json.set("fileSize", static_cast<double>(track.fileSize));
    json.set("mtime", static_cast<double>(track.mtime));
    json.set("addedAt", static_cast<double>(track.addedAt));
    json.set("lastPlayedAt", static_cast<double>(track.lastPlayedAt));
    json.set("playCount", track.playCount);
    json.set("favorite", track.favorite);
    json.set("rating", track.rating);
    json.set("cover", track.hasEmbeddedCover);
    json.set("source", static_cast<int>(track.source));
    json.set("sourceUrl", track.sourceUrl);
    json.set("coverPath", track.coverPath);
    json.set("lyricsPath", track.lyricsPath);
    return json;
}

Track trackFromJson(const Json& json) {
    Track track;
    track.id = json["id"].asString();
    track.path = json["path"].asString();
    track.title = json["title"].asString();
    track.artist = json["artist"].asString();
    track.album = json["album"].asString();
    track.albumArtist = json["albumArtist"].asString();
    track.genre = json["genre"].asString();
    track.year = json["year"].asString();
    track.trackNo = static_cast<int>(json["trackNo"].asInt());
    track.discNo = static_cast<int>(json["discNo"].asInt());
    track.durationSec = json["duration"].asDouble();
    track.sampleRate = static_cast<int>(json["sampleRate"].asInt());
    track.channels = static_cast<int>(json["channels"].asInt());
    track.bitrateKbps = static_cast<int>(json["bitrate"].asInt());
    track.fileSize = static_cast<std::uint64_t>(json["fileSize"].asDouble());
    track.mtime = static_cast<std::int64_t>(json["mtime"].asDouble());
    track.addedAt = static_cast<std::int64_t>(json["addedAt"].asDouble());
    track.lastPlayedAt = static_cast<std::int64_t>(json["lastPlayedAt"].asDouble());
    track.playCount = static_cast<int>(json["playCount"].asInt());
    track.favorite = json["favorite"].asBool();
    track.rating = static_cast<int>(json["rating"].asInt());
    track.hasEmbeddedCover = json["cover"].asBool();
    track.source = static_cast<SourceKind>(json["source"].asInt());
    track.sourceUrl = json["sourceUrl"].asString();
    track.coverPath = json["coverPath"].asString();
    track.lyricsPath = json["lyricsPath"].asString();
    return track;
}

/// Higher is better; 0 means "no match".
int scoreMatch(const Track& track, const std::string& needle, const std::string& needleTranslit) {
    auto rank = [&](const std::string& field, int exact, int prefix, int inside) {
        if (field.empty()) return 0;
        const std::string normalized = str::normalize(field);
        if (normalized == needle) return exact;
        if (str::startsWith(normalized, needle)) return prefix;
        if (normalized.find(needle) != std::string::npos) return inside;
        if (!needleTranslit.empty()) {
            const std::string transliterated = str::translit(normalized);
            if (transliterated.find(needleTranslit) != std::string::npos) return inside - 2;
        }
        return 0;
    };
    int score = 0;
    score = std::max(score, rank(track.title, 120, 95, 70));
    score = std::max(score, rank(track.artist, 100, 80, 60));
    score = std::max(score, rank(track.album, 90, 72, 52));
    score = std::max(score, rank(track.albumArtist, 70, 55, 40));
    score = std::max(score, rank(track.genre, 40, 30, 20));
    score = std::max(score, rank(str::fileName(track.path), 35, 25, 15));
    if (score > 0) {
        if (track.favorite) score += 6;
        score += std::min(track.playCount, 8);
    }
    return score;
}

std::int64_t fileTimeToInt(const fs::path& path) {
    std::error_code ec;
    const auto time = fs::last_write_time(path, ec);
    if (ec) return 0;
    return static_cast<std::int64_t>(time.time_since_epoch().count());
}

} // namespace

MediaLibrary::MediaLibrary(std::string indexPath) : indexPath_(std::move(indexPath)) {}

MediaLibrary::~MediaLibrary() {
    cancel_.store(true);
    if (worker_.joinable()) worker_.join();
}

std::int64_t MediaLibrary::nowSeconds() {
    return static_cast<std::int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

void MediaLibrary::setProgressCallback(ProgressCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    progress_ = std::move(callback);
}

void MediaLibrary::setDecoderTools(std::string ffmpegPath, std::string ffprobePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    ffmpegPath_ = std::move(ffmpegPath);
    ffprobePath_ = std::move(ffprobePath);
}

void MediaLibrary::reindexUnlocked() {
    byId_.clear();
    byPath_.clear();
    for (std::size_t i = 0; i < tracks_.size(); ++i) {
        byId_[tracks_[i].id] = i;
        byPath_[tracks_[i].path] = i;
    }
}

bool MediaLibrary::load() {
    if (indexPath_.empty()) return false;
    std::string error;
    const Json root = Json::parseFile(indexPath_, &error);
    if (!error.empty()) {
        logDebug("library", "no index yet (" + error + ")");
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    tracks_.clear();
    roots_.clear();
    const Json& trackArray = root["tracks"];
    for (std::size_t i = 0; i < trackArray.size(); ++i) {
        Track track = trackFromJson(trackArray.at(i));
        if (track.valid()) tracks_.push_back(track);
    }
    const Json& rootArray = root["roots"];
    for (std::size_t i = 0; i < rootArray.size(); ++i) {
        roots_.push_back(rootArray.at(i).asString());
    }
    reindexUnlocked();
    logInfo("library", "loaded " + std::to_string(tracks_.size()) + " tracks");
    return true;
}

bool MediaLibrary::save() const {
    if (indexPath_.empty()) return false;
    Json root = Json::object();
    Json trackArray = Json::array();
    Json rootArray = Json::array();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const Track& track : tracks_) trackArray.push(trackToJson(track));
        for (const std::string& root_ : roots_) rootArray.push(Json(root_));
    }
    root.set("version", kIndexVersion);
    root.set("savedAt", static_cast<double>(nowSeconds()));
    root.set("roots", rootArray);
    root.set("tracks", trackArray);

    std::error_code ec;
    fs::create_directories(fs::path(indexPath_).parent_path(), ec);
    return root.saveFile(indexPath_, 1);
}

bool MediaLibrary::readTrackUnlocked(const std::string& path, Track* out) const {
    if (!out) return false;
    Tags tags;
    std::string error;
    // ffprobe fallback paths come from the settings
    if (!TagReader::read(path, &tags, &error)) {
        logWarn("library", "skip " + path + ": " + error);
        return false;
    }
    out->path = path;
    out->id = str::hashId(path);
    out->title = tags.title.empty() ? str::stem(path) : tags.title;
    out->artist = tags.artist;
    out->album = tags.album;
    out->albumArtist = tags.albumArtist.empty() ? tags.artist : tags.albumArtist;
    out->genre = tags.genre;
    out->year = tags.year;
    out->trackNo = tags.trackNo;
    out->discNo = tags.discNo;
    out->durationSec = tags.durationSec;
    out->sampleRate = tags.sampleRate;
    out->channels = tags.channels;
    out->bitrateKbps = tags.bitrateKbps;
    out->hasEmbeddedCover = tags.hasCover;

    if (!str::isUrl(path)) {
        std::error_code ec;
        out->fileSize = static_cast<std::uint64_t>(fs::file_size(fs::path(path), ec));
        if (ec) out->fileSize = 0;
        out->mtime = fileTimeToInt(fs::path(path));
        if (!tags.hasCover) out->coverPath = TagReader::findSidecarCover(path);
        out->lyricsPath = TagReader::findSidecarLyrics(path);
        out->source = SourceKind::LocalFile;
    } else {
        out->source = SourceKind::Stream;
        out->sourceUrl = path;
    }
    return true;
}

ScanReport MediaLibrary::scan(const std::vector<std::string>& roots, bool incremental) {
    ScanReport report;
    const auto begin = std::chrono::steady_clock::now();
    cancel_.store(false);
    scanning_.store(true);

    std::vector<std::string> scanRoots = roots;
    if (scanRoots.empty()) {
        std::lock_guard<std::mutex> lock(mutex_);
        scanRoots = roots_;
    }

    ProgressCallback progress;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        progress = progress_;
        for (const std::string& root : scanRoots) {
            const std::string canonical = canonicalPath(root);
            if (std::find(roots_.begin(), roots_.end(), canonical) == roots_.end()) {
                roots_.push_back(canonical);
            }
        }
    }

    std::set<std::string> seenPaths;
    for (const std::string& root : scanRoots) {
        std::error_code ec;
        const fs::path rootPath(root);
        if (!fs::exists(rootPath, ec)) {
            logWarn("library", "root does not exist: " + root);
            continue;
        }
        if (fs::is_regular_file(rootPath, ec)) {
            Track track;
            if (addFile(root, &track)) ++report.added;
            seenPaths.insert(track.path);
            continue;
        }

        fs::recursive_directory_iterator it(
            rootPath, fs::directory_options::skip_permission_denied, ec);
        const fs::recursive_directory_iterator end;
        for (; it != end; it.increment(ec)) {
            if (cancel_.load()) {
                report.cancelled = true;
                break;
            }
            if (ec) {
                ec.clear();
                continue;
            }
            if (!it->is_regular_file(ec)) continue;
            const std::string path = it->path().string();
            if (!isSupportedAudioFile(path)) continue;
            ++report.filesSeen;
            if (progress && (report.filesSeen % 25 == 0)) progress(path, report.filesSeen);

            const std::string canonical = canonicalPath(path);
            seenPaths.insert(canonical);

            bool needsRead = true;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                const auto found = byPath_.find(canonical);
                if (incremental && found != byPath_.end()) {
                    const Track& existing = tracks_[found->second];
                    std::error_code sizeEc;
                    const auto size = fs::file_size(it->path(), sizeEc);
                    const std::int64_t mtime = fileTimeToInt(it->path());
                    if (!sizeEc && existing.fileSize == static_cast<std::uint64_t>(size) &&
                        existing.mtime == mtime) {
                        needsRead = false;
                        ++report.skipped;
                    }
                }
            }
            if (!needsRead) continue;

            Track track;
            if (!readTrackUnlocked(canonical, &track)) continue;
            std::lock_guard<std::mutex> lock(mutex_);
            const auto found = byPath_.find(canonical);
            if (found != byPath_.end()) {
                Track& existing = tracks_[found->second];
                // keep user data
                track.playCount = existing.playCount;
                track.favorite = existing.favorite;
                track.rating = existing.rating;
                track.addedAt = existing.addedAt;
                track.lastPlayedAt = existing.lastPlayedAt;
                existing = track;
                ++report.updated;
            } else {
                track.addedAt = nowSeconds();
                tracks_.push_back(track);
                byId_[track.id] = tracks_.size() - 1;
                byPath_[track.path] = tracks_.size() - 1;
                ++report.added;
            }
        }
        if (report.cancelled) break;
    }

    // Drop local tracks that disappeared from disk.
    if (!report.cancelled) {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::size_t before = tracks_.size();
        tracks_.erase(std::remove_if(tracks_.begin(), tracks_.end(),
                                     [&](const Track& track) {
                                         if (track.source == SourceKind::Stream) return false;
                                         std::error_code ec;
                                         return !fs::exists(fs::path(track.path), ec);
                                     }),
                      tracks_.end());
        report.removed = before - tracks_.size();
        reindexUnlocked();
    }

    scanning_.store(false);
    report.elapsedSec =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
    logInfo("library", "scan done: +" + std::to_string(report.added) + " ~" +
                           std::to_string(report.updated) + " -" + std::to_string(report.removed) +
                           " (" + str::formatDouble(report.elapsedSec, 2) + "s)");
    return report;
}

void MediaLibrary::scanAsync(const std::vector<std::string>& roots, bool incremental,
                             std::function<void(ScanReport)> onDone) {
    if (worker_.joinable()) worker_.join();
    const std::vector<std::string> copy = roots;
    worker_ = std::thread([this, copy, incremental, onDone] {
        const ScanReport report = scan(copy, incremental);
        if (!indexPath_.empty()) save();
        if (onDone) onDone(report);
    });
}

bool MediaLibrary::addFile(const std::string& path, Track* out) {
    const std::string canonical = canonicalPath(path);
    if (str::isUrl(canonical)) return addStream(canonical, std::string(), out);
    std::error_code ec;
    if (!fs::is_regular_file(fs::path(canonical), ec)) return false;
    if (!isSupportedAudioFile(canonical)) return false;

    Track track;
    if (!readTrackUnlocked(canonical, &track)) return false;

    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = byPath_.find(canonical);
    if (found != byPath_.end()) {
        Track& existing = tracks_[found->second];
        track.playCount = existing.playCount;
        track.favorite = existing.favorite;
        track.rating = existing.rating;
        track.addedAt = existing.addedAt;
        track.lastPlayedAt = existing.lastPlayedAt;
        existing = track;
        if (out) *out = existing;
        return true;
    }
    track.addedAt = nowSeconds();
    tracks_.push_back(track);
    byId_[track.id] = tracks_.size() - 1;
    byPath_[track.path] = tracks_.size() - 1;
    if (out) *out = track;
    return true;
}

bool MediaLibrary::addStream(const std::string& url, const std::string& title, Track* out) {
    if (!str::isUrl(url)) return false;
    Track track;
    track.path = url;
    track.id = str::hashId(url);
    track.title = title.empty() ? url : title;
    track.source = SourceKind::Stream;
    track.sourceUrl = url;
    track.addedAt = nowSeconds();

    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = byPath_.find(url);
    if (found != byPath_.end()) {
        if (out) *out = tracks_[found->second];
        return true;
    }
    tracks_.push_back(track);
    byId_[track.id] = tracks_.size() - 1;
    byPath_[track.path] = tracks_.size() - 1;
    if (out) *out = track;
    return true;
}

bool MediaLibrary::addDownloaded(const std::string& path, const std::string& sourceUrl, Track* out) {
    Track track;
    if (!addFile(path, &track)) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = byId_.find(track.id);
    if (found == byId_.end()) return false;
    tracks_[found->second].source = SourceKind::Downloaded;
    tracks_[found->second].sourceUrl = sourceUrl;
    if (out) *out = tracks_[found->second];
    return true;
}

bool MediaLibrary::remove(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = byId_.find(id);
    if (found == byId_.end()) return false;
    tracks_.erase(tracks_.begin() + static_cast<long>(found->second));
    reindexUnlocked();
    return true;
}

void MediaLibrary::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    tracks_.clear();
    byId_.clear();
    byPath_.clear();
}

std::size_t MediaLibrary::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tracks_.size();
}

std::vector<Track> MediaLibrary::tracks() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tracks_;
}

bool MediaLibrary::track(const std::string& id, Track* out) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = byId_.find(id);
    if (found == byId_.end()) return false;
    if (out) *out = tracks_[found->second];
    return true;
}

bool MediaLibrary::trackByPath(const std::string& path, Track* out) const {
    const std::string canonical = canonicalPath(path);
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = byPath_.find(canonical);
    if (found == byPath_.end()) return false;
    if (out) *out = tracks_[found->second];
    return true;
}

std::vector<Track> MediaLibrary::search(const std::string& query, std::size_t limit) const {
    const std::string needle = str::normalize(query);
    if (needle.empty()) return std::vector<Track>();
    const std::string needleTranslit = str::translit(needle);

    std::vector<std::pair<int, const Track*>> scored;
    std::lock_guard<std::mutex> lock(mutex_);
    scored.reserve(tracks_.size());
    for (const Track& track : tracks_) {
        const int score = scoreMatch(track, needle, needleTranslit);
        if (score > 0) scored.push_back(std::make_pair(score, &track));
    }
    std::stable_sort(scored.begin(), scored.end(),
                     [](const std::pair<int, const Track*>& a,
                        const std::pair<int, const Track*>& b) { return a.first > b.first; });

    std::vector<Track> results;
    for (const auto& entry : scored) {
        if (results.size() >= limit) break;
        results.push_back(*entry.second);
    }
    return results;
}

std::vector<AlbumInfo> MediaLibrary::albums() const {
    std::map<std::string, AlbumInfo> albums;
    std::lock_guard<std::mutex> lock(mutex_);
    for (const Track& track : tracks_) {
        // Files without an album tag are grouped by their folder, so the album
        // view never hides music that is present in the library.
        std::string albumName = track.album;
        if (albumName.empty()) albumName = str::fileName(str::parentDir(track.path));
        if (albumName.empty()) albumName = track.isStream() ? "Streams" : "Unknown album";
        const std::string key = str::toLower(albumName) + "\x1f" + str::toLower(track.albumArtist);
        AlbumInfo& album = albums[key];
        if (album.name.empty()) {
            album.name = albumName;
            album.artist = track.albumArtist.empty() ? track.artist : track.albumArtist;
            album.year = track.year;
        }
        ++album.trackCount;
        album.durationSec += track.durationSec;
        album.trackIds.push_back(track.id);
        if (album.coverPath.empty() && !track.coverPath.empty()) album.coverPath = track.coverPath;
    }
    std::vector<AlbumInfo> result;
    result.reserve(albums.size());
    for (const auto& entry : albums) result.push_back(entry.second);
    std::sort(result.begin(), result.end(), [](const AlbumInfo& a, const AlbumInfo& b) {
        return str::toLower(a.name) < str::toLower(b.name);
    });
    return result;
}

std::vector<ArtistInfo> MediaLibrary::artists() const {
    std::map<std::string, ArtistInfo> artists;
    std::map<std::string, std::set<std::string>> albumsByArtist;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const Track& track : tracks_) {
            const std::string name = track.artist.empty() ? track.albumArtist : track.artist;
            if (name.empty()) continue;
            const std::string key = str::toLower(name);
            ArtistInfo& artist = artists[key];
            if (artist.name.empty()) artist.name = name;
            ++artist.trackCount;
            artist.durationSec += track.durationSec;
            if (!track.album.empty()) albumsByArtist[key].insert(str::toLower(track.album));
        }
    }
    std::vector<ArtistInfo> result;
    result.reserve(artists.size());
    for (auto& entry : artists) {
        entry.second.albumCount = albumsByArtist[entry.first].size();
        result.push_back(entry.second);
    }
    std::sort(result.begin(), result.end(), [](const ArtistInfo& a, const ArtistInfo& b) {
        return str::toLower(a.name) < str::toLower(b.name);
    });
    return result;
}

std::vector<std::string> MediaLibrary::genres() const {
    std::set<std::string> unique;
    std::lock_guard<std::mutex> lock(mutex_);
    for (const Track& track : tracks_) {
        if (!track.genre.empty()) unique.insert(track.genre);
    }
    return std::vector<std::string>(unique.begin(), unique.end());
}

std::vector<Track> MediaLibrary::byAlbum(const std::string& album) const {
    const std::string needle = str::toLower(album);
    std::vector<Track> result;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const Track& track : tracks_) {
            if (str::toLower(track.album) == needle) result.push_back(track);
        }
    }
    std::sort(result.begin(), result.end(), [](const Track& a, const Track& b) {
        if (a.discNo != b.discNo) return a.discNo < b.discNo;
        if (a.trackNo != b.trackNo) return a.trackNo < b.trackNo;
        return a.title < b.title;
    });
    return result;
}

std::vector<Track> MediaLibrary::byArtist(const std::string& artist) const {
    const std::string needle = str::toLower(artist);
    std::vector<Track> result;
    std::lock_guard<std::mutex> lock(mutex_);
    for (const Track& track : tracks_) {
        if (str::toLower(track.artist) == needle || str::toLower(track.albumArtist) == needle) {
            result.push_back(track);
        }
    }
    return result;
}

std::vector<Track> MediaLibrary::byGenre(const std::string& genre) const {
    const std::string needle = str::toLower(genre);
    std::vector<Track> result;
    std::lock_guard<std::mutex> lock(mutex_);
    for (const Track& track : tracks_) {
        if (str::toLower(track.genre) == needle) result.push_back(track);
    }
    return result;
}

std::vector<Track> MediaLibrary::recentlyAdded(std::size_t limit) const {
    std::vector<Track> result = tracks();
    std::sort(result.begin(), result.end(),
              [](const Track& a, const Track& b) { return a.addedAt > b.addedAt; });
    if (result.size() > limit) result.resize(limit);
    return result;
}

std::vector<Track> MediaLibrary::recentlyPlayed(std::size_t limit) const {
    std::vector<Track> result;
    for (const Track& track : tracks()) {
        if (track.lastPlayedAt > 0) result.push_back(track);
    }
    std::sort(result.begin(), result.end(),
              [](const Track& a, const Track& b) { return a.lastPlayedAt > b.lastPlayedAt; });
    if (result.size() > limit) result.resize(limit);
    return result;
}

std::vector<Track> MediaLibrary::mostPlayed(std::size_t limit) const {
    std::vector<Track> result;
    for (const Track& track : tracks()) {
        if (track.playCount > 0) result.push_back(track);
    }
    std::sort(result.begin(), result.end(),
              [](const Track& a, const Track& b) { return a.playCount > b.playCount; });
    if (result.size() > limit) result.resize(limit);
    return result;
}

std::vector<Track> MediaLibrary::favorites() const {
    std::vector<Track> result;
    std::lock_guard<std::mutex> lock(mutex_);
    for (const Track& track : tracks_) {
        if (track.favorite) result.push_back(track);
    }
    return result;
}

std::vector<Track> MediaLibrary::neverPlayed(std::size_t limit) const {
    std::vector<Track> result;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const Track& track : tracks_) {
            if (track.playCount == 0) result.push_back(track);
        }
    }
    if (result.size() > limit) result.resize(limit);
    return result;
}

std::vector<Track> MediaLibrary::shuffled(std::size_t limit) const {
    std::vector<Track> result = tracks();
    std::mt19937 rng(static_cast<unsigned>(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    std::shuffle(result.begin(), result.end(), rng);
    if (result.size() > limit) result.resize(limit);
    return result;
}

void MediaLibrary::markPlayed(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = byId_.find(id);
    if (found == byId_.end()) return;
    Track& track = tracks_[found->second];
    ++track.playCount;
    track.lastPlayedAt = nowSeconds();
}

bool MediaLibrary::setFavorite(const std::string& id, bool favorite) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = byId_.find(id);
    if (found == byId_.end()) return false;
    tracks_[found->second].favorite = favorite;
    return true;
}

bool MediaLibrary::toggleFavorite(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = byId_.find(id);
    if (found == byId_.end()) return false;
    Track& track = tracks_[found->second];
    track.favorite = !track.favorite;
    return track.favorite;
}

bool MediaLibrary::setRating(const std::string& id, int rating) {
    if (rating < 0) rating = 0;
    if (rating > 5) rating = 5;
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = byId_.find(id);
    if (found == byId_.end()) return false;
    tracks_[found->second].rating = rating;
    return true;
}

bool MediaLibrary::updateTrack(const Track& track) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = byId_.find(track.id);
    if (found == byId_.end()) return false;
    tracks_[found->second] = track;
    return true;
}

LibraryStats MediaLibrary::stats() const {
    LibraryStats stats;
    std::set<std::string> albums;
    std::set<std::string> artists;
    std::lock_guard<std::mutex> lock(mutex_);
    for (const Track& track : tracks_) {
        ++stats.trackCount;
        stats.totalDurationSec += track.durationSec;
        stats.totalBytes += track.fileSize;
        if (track.source == SourceKind::Stream) ++stats.streamCount;
        if (!track.album.empty()) albums.insert(str::toLower(track.album));
        if (!track.artist.empty()) artists.insert(str::toLower(track.artist));
    }
    stats.albumCount = albums.size();
    stats.artistCount = artists.size();
    return stats;
}

std::vector<std::string> MediaLibrary::roots() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return roots_;
}

void MediaLibrary::setRoots(std::vector<std::string> roots) {
    std::lock_guard<std::mutex> lock(mutex_);
    roots_ = std::move(roots);
}

void MediaLibrary::addRoot(const std::string& root) {
    const std::string canonical = canonicalPath(root);
    std::lock_guard<std::mutex> lock(mutex_);
    if (std::find(roots_.begin(), roots_.end(), canonical) == roots_.end()) {
        roots_.push_back(canonical);
    }
}

} // namespace aurora
