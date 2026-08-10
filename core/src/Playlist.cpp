#include "aurora/Playlist.hpp"

#include "aurora/Json.hpp"
#include "aurora/Log.hpp"
#include "aurora/Strings.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace aurora {
namespace {

std::int64_t nowSeconds() {
    return static_cast<std::int64_t>(std::chrono::duration_cast<std::chrono::seconds>(
                                        std::chrono::system_clock::now().time_since_epoch())
                                        .count());
}

} // namespace

PlayQueue::PlayQueue()
    : rng_(static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count())) {}

void PlayQueue::rebuildOrderUnlocked(bool keepCurrentFirst) {
    const std::size_t currentTrack = (orderPos_ < order_.size()) ? order_[orderPos_] : 0;
    order_.resize(tracks_.size());
    for (std::size_t i = 0; i < order_.size(); ++i) order_[i] = i;
    if (shuffle_) {
        std::shuffle(order_.begin(), order_.end(), rng_);
        if (keepCurrentFirst && !tracks_.empty()) {
            const auto it = std::find(order_.begin(), order_.end(), currentTrack);
            if (it != order_.end()) std::iter_swap(order_.begin(), it);
        }
        orderPos_ = 0;
    } else {
        orderPos_ = currentTrack < order_.size() ? currentTrack : 0;
    }
}

void PlayQueue::setTracks(std::vector<Track> tracks, int startIndex) {
    std::lock_guard<std::mutex> lock(mutex_);
    tracks_ = std::move(tracks);
    history_.clear();
    started_ = !tracks_.empty();
    order_.resize(tracks_.size());
    for (std::size_t i = 0; i < order_.size(); ++i) order_[i] = i;
    if (shuffle_) {
        std::shuffle(order_.begin(), order_.end(), rng_);
        if (startIndex >= 0 && static_cast<std::size_t>(startIndex) < tracks_.size()) {
            const auto it = std::find(order_.begin(), order_.end(),
                                      static_cast<std::size_t>(startIndex));
            if (it != order_.end()) std::iter_swap(order_.begin(), it);
        }
        orderPos_ = 0;
    } else {
        orderPos_ = (startIndex > 0 && static_cast<std::size_t>(startIndex) < tracks_.size())
                        ? static_cast<std::size_t>(startIndex)
                        : 0;
    }
}

void PlayQueue::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    tracks_.clear();
    order_.clear();
    history_.clear();
    orderPos_ = 0;
    started_ = false;
}

void PlayQueue::add(const Track& track) {
    std::lock_guard<std::mutex> lock(mutex_);
    tracks_.push_back(track);
    order_.push_back(tracks_.size() - 1);
    if (!started_) {
        started_ = true;
        orderPos_ = order_.size() - 1;
    }
}

void PlayQueue::addAll(const std::vector<Track>& tracks) {
    for (const Track& track : tracks) add(track);
}

void PlayQueue::insertNext(const Track& track) {
    std::lock_guard<std::mutex> lock(mutex_);
    tracks_.push_back(track);
    const std::size_t index = tracks_.size() - 1;
    if (order_.empty()) {
        order_.push_back(index);
        orderPos_ = 0;
        started_ = true;
    } else {
        order_.insert(order_.begin() + static_cast<long>(orderPos_ + 1), index);
    }
}

bool PlayQueue::removeAt(std::size_t index) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (index >= tracks_.size()) return false;
    const std::size_t currentTrack = (orderPos_ < order_.size()) ? order_[orderPos_] : 0;
    tracks_.erase(tracks_.begin() + static_cast<long>(index));

    order_.erase(std::remove(order_.begin(), order_.end(), index), order_.end());
    for (std::size_t& value : order_) {
        if (value > index) --value;
    }
    std::size_t newCurrent = currentTrack;
    if (currentTrack == index) {
        newCurrent = currentTrack < tracks_.size() ? currentTrack : 0;
    } else if (currentTrack > index) {
        newCurrent = currentTrack - 1;
    }
    const auto it = std::find(order_.begin(), order_.end(), newCurrent);
    orderPos_ = (it != order_.end()) ? static_cast<std::size_t>(it - order_.begin()) : 0;
    if (tracks_.empty()) started_ = false;
    return true;
}

bool PlayQueue::move(std::size_t from, std::size_t to) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (from >= tracks_.size() || to >= tracks_.size() || from == to) return false;
    Track track = tracks_[from];
    tracks_.erase(tracks_.begin() + static_cast<long>(from));
    tracks_.insert(tracks_.begin() + static_cast<long>(to), track);
    // Order indices become meaningless after a manual reorder: rebuild them.
    const std::size_t currentTrack = to;
    order_.resize(tracks_.size());
    for (std::size_t i = 0; i < order_.size(); ++i) order_[i] = i;
    if (shuffle_) {
        std::shuffle(order_.begin(), order_.end(), rng_);
        const auto it = std::find(order_.begin(), order_.end(), currentTrack);
        if (it != order_.end()) std::iter_swap(order_.begin(), it);
        orderPos_ = 0;
    } else {
        orderPos_ = currentTrack;
    }
    return true;
}

std::vector<Track> PlayQueue::tracks() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tracks_;
}

std::size_t PlayQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tracks_.size();
}

bool PlayQueue::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tracks_.empty();
}

int PlayQueue::currentIndex() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!started_ || orderPos_ >= order_.size()) return -1;
    return static_cast<int>(order_[orderPos_]);
}

bool PlayQueue::current(Track* out) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!started_ || orderPos_ >= order_.size()) return false;
    if (out) *out = tracks_[order_[orderPos_]];
    return true;
}

bool PlayQueue::setCurrentIndex(int index) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (index < 0 || static_cast<std::size_t>(index) >= tracks_.size()) return false;
    const auto it = std::find(order_.begin(), order_.end(), static_cast<std::size_t>(index));
    if (it == order_.end()) return false;
    if (started_ && orderPos_ < order_.size()) history_.push_back(order_[orderPos_]);
    orderPos_ = static_cast<std::size_t>(it - order_.begin());
    started_ = true;
    return true;
}

bool PlayQueue::trackAt(std::size_t index, Track* out) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (index >= tracks_.size()) return false;
    if (out) *out = tracks_[index];
    return true;
}

int PlayQueue::nextOrderPositionUnlocked(bool userInitiated) const {
    if (order_.empty()) return -1;
    if (!userInitiated && repeat_ == RepeatMode::One) return static_cast<int>(orderPos_);
    if (orderPos_ + 1 < order_.size()) return static_cast<int>(orderPos_ + 1);
    if (repeat_ == RepeatMode::All || (userInitiated && repeat_ == RepeatMode::One)) return 0;
    return -1;
}

bool PlayQueue::next(Track* out, bool userInitiated) {
    std::lock_guard<std::mutex> lock(mutex_);
    const int position = nextOrderPositionUnlocked(userInitiated);
    if (position < 0) return false;
    if (orderPos_ < order_.size()) history_.push_back(order_[orderPos_]);
    if (position == 0 && orderPos_ + 1 >= order_.size() && shuffle_) {
        // reshuffle when a shuffled pass completes
        std::shuffle(order_.begin(), order_.end(), rng_);
    }
    orderPos_ = static_cast<std::size_t>(position);
    started_ = true;
    if (out) *out = tracks_[order_[orderPos_]];
    return true;
}

bool PlayQueue::previous(Track* out) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!history_.empty()) {
        const std::size_t trackIndex = history_.back();
        history_.pop_back();
        const auto it = std::find(order_.begin(), order_.end(), trackIndex);
        if (it != order_.end()) {
            orderPos_ = static_cast<std::size_t>(it - order_.begin());
            if (out) *out = tracks_[order_[orderPos_]];
            return true;
        }
    }
    if (order_.empty()) return false;
    if (orderPos_ > 0) {
        --orderPos_;
    } else if (repeat_ == RepeatMode::All) {
        orderPos_ = order_.size() - 1;
    } else {
        return false;
    }
    if (out) *out = tracks_[order_[orderPos_]];
    return true;
}

bool PlayQueue::peekNext(Track* out) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const int position = nextOrderPositionUnlocked(false);
    if (position < 0) return false;
    if (out) *out = tracks_[order_[static_cast<std::size_t>(position)]];
    return true;
}

std::vector<Track> PlayQueue::upcoming(std::size_t limit) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Track> result;
    if (order_.empty()) return result;
    for (std::size_t i = 1; i <= order_.size() && result.size() < limit; ++i) {
        std::size_t position = orderPos_ + i;
        if (position >= order_.size()) {
            if (repeat_ != RepeatMode::All) break;
            position %= order_.size();
        }
        result.push_back(tracks_[order_[position]]);
    }
    return result;
}

void PlayQueue::setRepeat(RepeatMode mode) {
    std::lock_guard<std::mutex> lock(mutex_);
    repeat_ = mode;
}

RepeatMode PlayQueue::repeat() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return repeat_;
}

void PlayQueue::setShuffle(bool shuffle) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (shuffle_ == shuffle) return;
    shuffle_ = shuffle;
    rebuildOrderUnlocked(true);
}

bool PlayQueue::shuffle() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return shuffle_;
}

bool PlayQueue::loadM3u(const std::string& path, std::vector<std::string>* pathsOut) {
    std::ifstream in(path);
    if (!in) return false;
    const std::string base = str::parentDir(path);
    std::string line;
    std::vector<std::string> paths;
    while (std::getline(in, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);
        const std::string entry = str::trim(line);
        if (entry.empty() || entry[0] == '#') continue;
        if (str::isUrl(entry)) {
            paths.push_back(entry);
        } else if (!entry.empty() && (entry[0] == '/' || entry.find(':') == 1)) {
            paths.push_back(entry);
        } else {
            paths.push_back(str::joinPath(base, entry));
        }
    }
    if (pathsOut) *pathsOut = paths;
    return !paths.empty();
}

bool PlayQueue::saveM3u(const std::string& path) const {
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << "#EXTM3U\n";
    std::lock_guard<std::mutex> lock(mutex_);
    for (const Track& track : tracks_) {
        out << "#EXTINF:" << static_cast<long long>(track.durationSec) << ",";
        if (!track.artist.empty()) out << track.artist << " - ";
        out << track.title << "\n";
        out << track.path << "\n";
    }
    return true;
}

// ---------------------------------------------------------------------------
// PlaylistStore
// ---------------------------------------------------------------------------
PlaylistStore::PlaylistStore(std::string path) : path_(std::move(path)) {}

bool PlaylistStore::load() {
    if (path_.empty()) return false;
    std::string error;
    const Json root = Json::parseFile(path_, &error);
    if (!error.empty()) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    playlists_.clear();
    const Json& array = root["playlists"];
    for (std::size_t i = 0; i < array.size(); ++i) {
        const Json& item = array.at(i);
        Playlist playlist;
        playlist.id = item["id"].asString();
        playlist.name = item["name"].asString();
        playlist.createdAt = static_cast<std::int64_t>(item["createdAt"].asDouble());
        playlist.updatedAt = static_cast<std::int64_t>(item["updatedAt"].asDouble());
        const Json& ids = item["tracks"];
        for (std::size_t j = 0; j < ids.size(); ++j) {
            playlist.trackIds.push_back(ids.at(j).asString());
        }
        if (!playlist.id.empty()) playlists_.push_back(playlist);
    }
    return true;
}

bool PlaylistStore::save() const {
    if (path_.empty()) return false;
    Json root = Json::object();
    Json array = Json::array();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const Playlist& playlist : playlists_) {
            Json item = Json::object();
            item.set("id", playlist.id);
            item.set("name", playlist.name);
            item.set("createdAt", static_cast<double>(playlist.createdAt));
            item.set("updatedAt", static_cast<double>(playlist.updatedAt));
            Json ids = Json::array();
            for (const std::string& id : playlist.trackIds) ids.push(Json(id));
            item.set("tracks", ids);
            array.push(item);
        }
    }
    root.set("version", 1);
    root.set("playlists", array);
    std::error_code ec;
    fs::create_directories(fs::path(path_).parent_path(), ec);
    return root.saveFile(path_, 1);
}

std::string PlaylistStore::create(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    Playlist playlist;
    playlist.name = name.empty() ? "Playlist" : name;
    playlist.id = str::hashId(playlist.name + std::to_string(nowSeconds()) +
                              std::to_string(playlists_.size()));
    playlist.createdAt = nowSeconds();
    playlist.updatedAt = playlist.createdAt;
    playlists_.push_back(playlist);
    return playlist.id;
}

bool PlaylistStore::rename(const std::string& id, const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (Playlist& playlist : playlists_) {
        if (playlist.id == id) {
            playlist.name = name;
            playlist.updatedAt = nowSeconds();
            return true;
        }
    }
    return false;
}

bool PlaylistStore::remove(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::size_t before = playlists_.size();
    playlists_.erase(std::remove_if(playlists_.begin(), playlists_.end(),
                                    [&](const Playlist& p) { return p.id == id; }),
                     playlists_.end());
    return playlists_.size() != before;
}

bool PlaylistStore::addTracks(const std::string& id, const std::vector<std::string>& trackIds) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (Playlist& playlist : playlists_) {
        if (playlist.id != id) continue;
        for (const std::string& trackId : trackIds) {
            if (std::find(playlist.trackIds.begin(), playlist.trackIds.end(), trackId) ==
                playlist.trackIds.end()) {
                playlist.trackIds.push_back(trackId);
            }
        }
        playlist.updatedAt = nowSeconds();
        return true;
    }
    return false;
}

bool PlaylistStore::removeTrack(const std::string& id, const std::string& trackId) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (Playlist& playlist : playlists_) {
        if (playlist.id != id) continue;
        const std::size_t before = playlist.trackIds.size();
        playlist.trackIds.erase(
            std::remove(playlist.trackIds.begin(), playlist.trackIds.end(), trackId),
            playlist.trackIds.end());
        playlist.updatedAt = nowSeconds();
        return playlist.trackIds.size() != before;
    }
    return false;
}

bool PlaylistStore::moveTrack(const std::string& id, std::size_t from, std::size_t to) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (Playlist& playlist : playlists_) {
        if (playlist.id != id) continue;
        if (from >= playlist.trackIds.size() || to >= playlist.trackIds.size()) return false;
        const std::string value = playlist.trackIds[from];
        playlist.trackIds.erase(playlist.trackIds.begin() + static_cast<long>(from));
        playlist.trackIds.insert(playlist.trackIds.begin() + static_cast<long>(to), value);
        playlist.updatedAt = nowSeconds();
        return true;
    }
    return false;
}

bool PlaylistStore::get(const std::string& id, Playlist* out) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const Playlist& playlist : playlists_) {
        if (playlist.id == id) {
            if (out) *out = playlist;
            return true;
        }
    }
    return false;
}

bool PlaylistStore::findByName(const std::string& name, Playlist* out) const {
    const std::string needle = str::toLower(name);
    std::lock_guard<std::mutex> lock(mutex_);
    for (const Playlist& playlist : playlists_) {
        if (str::toLower(playlist.name) == needle) {
            if (out) *out = playlist;
            return true;
        }
    }
    return false;
}

std::vector<Playlist> PlaylistStore::all() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return playlists_;
}

std::size_t PlaylistStore::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return playlists_.size();
}

} // namespace aurora
