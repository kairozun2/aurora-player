#include "aurora/Config.hpp"

#include "aurora/Equalizer.hpp"
#include "aurora/Json.hpp"
#include "aurora/Log.hpp"
#include "aurora/Strings.hpp"

#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

namespace aurora {
namespace {

const char* kAppFolder = "aurora-player";

std::string envOrEmpty(const char* name) {
    const char* value = std::getenv(name);
    return (value && *value) ? std::string(value) : std::string();
}

} // namespace

Config::Config() : Config(str::joinPath(configDir(), "settings.json")) {}

Config::Config(std::string path) : path_(std::move(path)) {
    settings_.downloadDir = defaultDownloadDir();
    settings_.equalizerGains.assign(static_cast<std::size_t>(Equalizer::kBands), 0.0f);
    const std::string music = defaultMusicDir();
    if (!music.empty()) settings_.musicFolders.push_back(music);
}

std::string Config::homeDir() {
#ifdef _WIN32
    const std::string profile = envOrEmpty("USERPROFILE");
    if (!profile.empty()) return profile;
    return envOrEmpty("HOMEDRIVE") + envOrEmpty("HOMEPATH");
#else
    const std::string home = envOrEmpty("HOME");
    return home.empty() ? std::string(".") : home;
#endif
}

std::string Config::configDir() {
    const std::string override = envOrEmpty("AURORA_CONFIG_DIR");
    if (!override.empty()) return override;
#ifdef _WIN32
    const std::string appData = envOrEmpty("APPDATA");
    return str::joinPath(appData.empty() ? homeDir() : appData, "AuroraPlayer");
#elif defined(__APPLE__)
    return str::joinPath(str::joinPath(homeDir(), "Library/Application Support"), "AuroraPlayer");
#else
    const std::string xdg = envOrEmpty("XDG_CONFIG_HOME");
    return str::joinPath(xdg.empty() ? str::joinPath(homeDir(), ".config") : xdg, kAppFolder);
#endif
}

std::string Config::dataDir() {
    const std::string override = envOrEmpty("AURORA_DATA_DIR");
    if (!override.empty()) return override;
#ifdef _WIN32
    const std::string local = envOrEmpty("LOCALAPPDATA");
    return str::joinPath(local.empty() ? homeDir() : local, "AuroraPlayer");
#elif defined(__APPLE__)
    return str::joinPath(str::joinPath(homeDir(), "Library/Application Support"), "AuroraPlayer");
#else
    const std::string xdg = envOrEmpty("XDG_DATA_HOME");
    return str::joinPath(xdg.empty() ? str::joinPath(homeDir(), ".local/share") : xdg, kAppFolder);
#endif
}

std::string Config::cacheDir() {
    const std::string override = envOrEmpty("AURORA_CACHE_DIR");
    if (!override.empty()) return override;
#ifdef _WIN32
    return str::joinPath(dataDir(), "cache");
#elif defined(__APPLE__)
    return str::joinPath(str::joinPath(homeDir(), "Library/Caches"), "AuroraPlayer");
#else
    const std::string xdg = envOrEmpty("XDG_CACHE_HOME");
    return str::joinPath(xdg.empty() ? str::joinPath(homeDir(), ".cache") : xdg, kAppFolder);
#endif
}

std::string Config::defaultMusicDir() {
    const std::string music = str::joinPath(homeDir(), "Music");
    std::error_code ec;
    if (fs::is_directory(fs::path(music), ec)) return music;
    const std::string russian = str::joinPath(homeDir(), "Музыка");
    if (fs::is_directory(fs::path(russian), ec)) return russian;
    return std::string();
}

std::string Config::defaultDownloadDir() {
    return str::joinPath(dataDir(), "downloads");
}

bool Config::ensureDir(const std::string& dir) {
    if (dir.empty()) return false;
    std::error_code ec;
    fs::create_directories(fs::path(dir), ec);
    return !ec;
}

std::string Config::libraryIndexPath() const { return str::joinPath(dataDir(), "library.json"); }
std::string Config::playlistsPath() const { return str::joinPath(dataDir(), "playlists.json"); }
std::string Config::coverCacheDir() const { return str::joinPath(cacheDir(), "covers"); }
std::string Config::logPath() const { return str::joinPath(dataDir(), "aurora.log"); }

bool Config::load() {
    std::string error;
    const Json root = Json::parseFile(path_, &error);
    if (!error.empty()) {
        logInfo("config", "using defaults (" + error + ")");
        return false;
    }

    Settings& s = settings_;
    s.language = root["language"].asString(s.language);
    s.theme = root["theme"].asString(s.theme);
    s.windowWidth = static_cast<int>(root["windowWidth"].asInt(s.windowWidth));
    s.windowHeight = static_cast<int>(root["windowHeight"].asInt(s.windowHeight));
    s.showVisualizer = root["showVisualizer"].asBool(s.showVisualizer);

    s.volume = static_cast<float>(root["volume"].asDouble(s.volume));
    s.muted = root["muted"].asBool(s.muted);
    s.speed = root["speed"].asDouble(s.speed);
    s.repeat = repeatFromString(root["repeat"].asString(toString(s.repeat)));
    s.shuffle = root["shuffle"].asBool(s.shuffle);
    s.crossfadeSec = root["crossfade"].asDouble(s.crossfadeSec);
    s.gapless = root["gapless"].asBool(s.gapless);
    s.rememberPosition = root["rememberPosition"].asBool(s.rememberPosition);
    s.lastTrackPath = root["lastTrack"].asString(s.lastTrackPath);
    s.lastPositionSec = root["lastPosition"].asDouble(s.lastPositionSec);

    s.sampleRate = static_cast<int>(root["sampleRate"].asInt(s.sampleRate));
    s.channels = static_cast<int>(root["channels"].asInt(s.channels));

    s.equalizerEnabled = root["equalizerEnabled"].asBool(s.equalizerEnabled);
    s.equalizerPreset = root["equalizerPreset"].asString(s.equalizerPreset);
    s.preampDb = static_cast<float>(root["preampDb"].asDouble(s.preampDb));
    const Json& gains = root["equalizerGains"];
    if (gains.isArray() && gains.size() > 0) {
        s.equalizerGains.clear();
        for (std::size_t i = 0; i < gains.size(); ++i) {
            s.equalizerGains.push_back(static_cast<float>(gains.at(i).asDouble()));
        }
    }

    const Json& folders = root["musicFolders"];
    if (folders.isArray()) {
        s.musicFolders.clear();
        for (std::size_t i = 0; i < folders.size(); ++i) {
            const std::string folder = folders.at(i).asString();
            if (!folder.empty()) s.musicFolders.push_back(folder);
        }
    }
    s.downloadDir = root["downloadDir"].asString(s.downloadDir);
    s.scanOnStart = root["scanOnStart"].asBool(s.scanOnStart);

    s.ffmpegPath = root["ffmpegPath"].asString(s.ffmpegPath);
    s.ffprobePath = root["ffprobePath"].asString(s.ffprobePath);
    s.ytdlpPath = root["ytdlpPath"].asString(s.ytdlpPath);
    return true;
}

bool Config::save() const {
    const Settings& s = settings_;
    Json root = Json::object();
    root.set("version", 1);
    root.set("language", s.language);
    root.set("theme", s.theme);
    root.set("windowWidth", s.windowWidth);
    root.set("windowHeight", s.windowHeight);
    root.set("showVisualizer", s.showVisualizer);

    root.set("volume", static_cast<double>(s.volume));
    root.set("muted", s.muted);
    root.set("speed", s.speed);
    root.set("repeat", std::string(toString(s.repeat)));
    root.set("shuffle", s.shuffle);
    root.set("crossfade", s.crossfadeSec);
    root.set("gapless", s.gapless);
    root.set("rememberPosition", s.rememberPosition);
    root.set("lastTrack", s.lastTrackPath);
    root.set("lastPosition", s.lastPositionSec);

    root.set("sampleRate", s.sampleRate);
    root.set("channels", s.channels);

    root.set("equalizerEnabled", s.equalizerEnabled);
    root.set("equalizerPreset", s.equalizerPreset);
    root.set("preampDb", static_cast<double>(s.preampDb));
    Json gains = Json::array();
    for (const float gain : s.equalizerGains) gains.push(Json(static_cast<double>(gain)));
    root.set("equalizerGains", gains);

    Json folders = Json::array();
    for (const std::string& folder : s.musicFolders) folders.push(Json(folder));
    root.set("musicFolders", folders);
    root.set("downloadDir", s.downloadDir);
    root.set("scanOnStart", s.scanOnStart);

    root.set("ffmpegPath", s.ffmpegPath);
    root.set("ffprobePath", s.ffprobePath);
    root.set("ytdlpPath", s.ytdlpPath);

    ensureDir(str::parentDir(path_));
    return root.saveFile(path_, 2);
}

} // namespace aurora
