// Aurora Player - persistent settings (JSON in the platform config folder).
#pragma once

#include "aurora/Types.hpp"

#include <string>
#include <vector>

namespace aurora {

struct Settings {
    // interface
    std::string language = "system";   ///< "system" | "ru" | "en"
    std::string theme = "dark";        ///< "dark" | "light" | "auto"
    int windowWidth = 1280;
    int windowHeight = 820;
    bool showVisualizer = true;

    // playback
    float volume = 0.85f;
    bool muted = false;
    double speed = 1.0;
    RepeatMode repeat = RepeatMode::Off;
    bool shuffle = false;
    double crossfadeSec = 0.0;
    bool gapless = true;
    bool rememberPosition = true;
    std::string lastTrackPath;
    double lastPositionSec = 0.0;

    // audio output
    int sampleRate = 48000;
    int channels = 2;

    // equalizer
    bool equalizerEnabled = false;
    std::string equalizerPreset = "flat";
    std::vector<float> equalizerGains;
    float preampDb = 0.0f;

    // content
    std::vector<std::string> musicFolders;
    std::string downloadDir;
    bool scanOnStart = true;

    // external tools
    std::string ffmpegPath = "ffmpeg";
    std::string ffprobePath = "ffprobe";
    std::string ytdlpPath = "yt-dlp";
};

class Config {
public:
    Config();
    explicit Config(std::string path);

    bool load();
    bool save() const;

    Settings& settings() { return settings_; }
    const Settings& settings() const { return settings_; }
    const std::string& path() const { return path_; }

    // Platform locations (created on demand).
    static std::string configDir();
    static std::string dataDir();
    static std::string cacheDir();
    static std::string homeDir();
    static std::string defaultMusicDir();
    static std::string defaultDownloadDir();
    static bool ensureDir(const std::string& dir);

    std::string libraryIndexPath() const;
    std::string playlistsPath() const;
    std::string coverCacheDir() const;
    std::string logPath() const;

private:
    std::string path_;
    Settings settings_;
};

} // namespace aurora
