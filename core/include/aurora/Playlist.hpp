// Aurora Player - play queue (shuffle/repeat/history) and persistent playlists.
#pragma once

#include "aurora/Types.hpp"

#include <cstdint>
#include <mutex>
#include <random>
#include <string>
#include <vector>

namespace aurora {

class PlayQueue {
public:
    PlayQueue();

    void setTracks(std::vector<Track> tracks, int startIndex = 0);
    void clear();
    void add(const Track& track);
    void addAll(const std::vector<Track>& tracks);
    /// Inserts right after the current track ("play next").
    void insertNext(const Track& track);
    bool removeAt(std::size_t index);
    bool move(std::size_t from, std::size_t to);

    std::vector<Track> tracks() const;
    std::size_t size() const;
    bool empty() const;

    int currentIndex() const;
    bool current(Track* out) const;
    bool setCurrentIndex(int index);
    bool trackAt(std::size_t index, Track* out) const;

    /// Advances according to repeat/shuffle. `userInitiated` ignores RepeatMode::One.
    bool next(Track* out, bool userInitiated = true);
    bool previous(Track* out);
    /// The track that will play next, without changing the queue state.
    bool peekNext(Track* out) const;
    std::vector<Track> upcoming(std::size_t limit = 20) const;

    void setRepeat(RepeatMode mode);
    RepeatMode repeat() const;
    void setShuffle(bool shuffle);
    bool shuffle() const;

    bool loadM3u(const std::string& path, std::vector<std::string>* pathsOut = nullptr);
    bool saveM3u(const std::string& path) const;

private:
    void rebuildOrderUnlocked(bool keepCurrentFirst);
    int nextOrderPositionUnlocked(bool userInitiated) const;

    mutable std::mutex mutex_;
    std::vector<Track> tracks_;
    std::vector<std::size_t> order_;   ///< playback order (identity or shuffled)
    std::size_t orderPos_ = 0;
    std::vector<std::size_t> history_;
    RepeatMode repeat_ = RepeatMode::Off;
    bool shuffle_ = false;
    bool started_ = false;
    std::mt19937 rng_;
};

struct Playlist {
    std::string id;
    std::string name;
    std::vector<std::string> trackIds;
    std::int64_t createdAt = 0;
    std::int64_t updatedAt = 0;
};

class PlaylistStore {
public:
    explicit PlaylistStore(std::string path = std::string());

    bool load();
    bool save() const;
    void setPath(std::string path) { path_ = std::move(path); }

    std::string create(const std::string& name);
    bool rename(const std::string& id, const std::string& name);
    bool remove(const std::string& id);
    bool addTracks(const std::string& id, const std::vector<std::string>& trackIds);
    bool removeTrack(const std::string& id, const std::string& trackId);
    bool moveTrack(const std::string& id, std::size_t from, std::size_t to);
    bool get(const std::string& id, Playlist* out) const;
    bool findByName(const std::string& name, Playlist* out) const;
    std::vector<Playlist> all() const;
    std::size_t size() const;

private:
    mutable std::mutex mutex_;
    std::string path_;
    std::vector<Playlist> playlists_;
};

} // namespace aurora
