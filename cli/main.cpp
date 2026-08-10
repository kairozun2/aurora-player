// Aurora Player - command line front-end.
//
// The CLI drives exactly the same core as the desktop UI, which makes the
// engine testable on machines without a GUI (CI, servers, this sandbox).
//
//   aurora-cli play song.mp3 --seconds 10
//   aurora-cli scan ~/Music && aurora-cli list --limit 20
//   aurora-cli add "https://www.youtube.com/watch?v=..."
//   aurora-cli demo --seconds 12 --out demo.wav
#include "aurora/Analysis.hpp"
#include "aurora/Config.hpp"
#include "aurora/Controller.hpp"
#include "aurora/Decoder.hpp"
#include "aurora/Downloader.hpp"
#include "aurora/I18n.hpp"
#include "aurora/Log.hpp"
#include "aurora/Process.hpp"
#include "aurora/Sink.hpp"
#include "aurora/Strings.hpp"
#include "aurora/TagReader.hpp"
#include "aurora/Version.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <vector>

using namespace aurora;

namespace {

struct Options {
    std::vector<std::string> positional;
    std::map<std::string, std::string> flags;

    bool has(const std::string& name) const { return flags.count(name) > 0; }
    std::string value(const std::string& name, const std::string& fallback = std::string()) const {
        const auto found = flags.find(name);
        return found == flags.end() ? fallback : found->second;
    }
    double number(const std::string& name, double fallback) const {
        const auto found = flags.find(name);
        if (found == flags.end() || found->second.empty()) return fallback;
        return std::strtod(found->second.c_str(), nullptr);
    }
};

Options parseOptions(const std::vector<std::string>& args) {
    Options options;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (str::startsWith(arg, "--")) {
            const std::string name = arg.substr(2);
            const std::size_t equals = name.find('=');
            if (equals != std::string::npos) {
                options.flags[name.substr(0, equals)] = name.substr(equals + 1);
            } else if (i + 1 < args.size() && !str::startsWith(args[i + 1], "--")) {
                options.flags[name] = args[i + 1];
                ++i;
            } else {
                options.flags[name] = "1";
            }
        } else {
            options.positional.push_back(arg);
        }
    }
    return options;
}

const char* kReset = "\033[0m";
const char* kBold = "\033[1m";
const char* kDim = "\033[2m";
const char* kAccent = "\033[38;5;215m";

void banner() {
    std::cout << kAccent << kBold << "  \u25cf Aurora Player " << versionString() << kReset
              << kDim << "  —  " << tr("app.tagline") << kReset << "\n";
}

void printHelp() {
    banner();
    std::cout << "\n" << kBold << "Usage:" << kReset << " aurora-cli <command> [options]\n\n";
    struct Row {
        const char* command;
        const char* description;
    };
    const Row rows[] = {
        {"play <file|url>", "play a file, folder or stream"},
        {"add <file|folder|url>", "add to the library (YouTube links are downloaded)"},
        {"scan [folders...]", "index music folders"},
        {"list [--limit N]", "list library tracks"},
        {"search <query>", "ranked search (RU/EN, transliteration aware)"},
        {"albums | artists", "browse the library"},
        {"info <file>", "show tags, duration, bitrate, cover"},
        {"waveform <file>", "draw an ASCII waveform"},
        {"eq [preset]", "list or apply an equalizer preset"},
        {"download <url>", "download audio via yt-dlp"},
        {"demo [--seconds N]", "play the library with crossfade and live status"},
        {"bench <file>", "decode benchmark (realtime factor)"},
        {"stats", "library and configuration summary"},
        {"lang <ru|en>", "switch interface language"},
        {"doctor", "check ffmpeg / yt-dlp availability"},
        {"version", "print version"},
    };
    for (const Row& row : rows) {
        std::cout << "  " << kBold << row.command << kReset;
        const int pad = 26 - static_cast<int>(std::string(row.command).size());
        for (int i = 0; i < std::max(1, pad); ++i) std::cout << ' ';
        std::cout << kDim << row.description << kReset << "\n";
    }
    std::cout << "\n" << kBold << "Common options:" << kReset << "\n"
              << kDim
              << "  --lang ru|en      --seconds N      --out file.wav\n"
              << "  --volume 0..1     --speed 0.5..2   --crossfade N\n"
              << "  --eq preset       --shuffle        --repeat off|all|one\n"
              << "  --fast N          render N times faster than real time\n"
              << kReset << "\n";
}

std::unique_ptr<ISink> makeSink(const Options& options, double maxSeconds) {
    const std::string out = options.value("out");
    if (!out.empty()) return makeWavFileSink(out, maxSeconds);
    return makeNullSink(options.number("fast", 1.0));
}

void applyPlaybackFlags(Controller& controller, const Options& options) {
    if (options.has("volume")) controller.setVolume(static_cast<float>(options.number("volume", 0.85)));
    if (options.has("speed")) controller.setSpeed(options.number("speed", 1.0));
    if (options.has("crossfade")) controller.setCrossfade(options.number("crossfade", 0.0));
    if (options.has("shuffle")) controller.setShuffle(true);
    if (options.has("repeat")) controller.setRepeat(repeatFromString(options.value("repeat", "off")));
    if (options.has("eq")) {
        controller.setEqualizerEnabled(true);
        controller.applyEqualizerPreset(options.value("eq", "flat"));
    }
    if (options.has("gapless")) controller.setGapless(options.value("gapless") != "0");
}

/// Prints a single-line, self-updating status bar.
void printStatus(Controller& controller, bool finalLine = false) {
    const PlayerSnapshot snapshot = controller.snapshot();
    const double duration = snapshot.durationSec > 0 ? snapshot.durationSec : snapshot.track.durationSec;
    const int width = 28;
    const double ratio = duration > 0 ? std::min(1.0, snapshot.positionSec / duration) : 0.0;
    const int filled = static_cast<int>(ratio * width);

    std::string title = snapshot.track.displayTitle();
    std::string artist = snapshot.track.displayArtist();
    if (artist.empty()) artist = tr("common.unknownArtist");

    std::cout << "\r" << (snapshot.state == PlaybackState::Playing ? "\u25b6" : "\u23f8") << "  "
              << kBold << str::ellipsize(title, 26) << kReset << kDim << "  " << str::ellipsize(artist, 18)
              << kReset << "  ";
    std::cout << kAccent;
    for (int i = 0; i < width; ++i) std::cout << (i < filled ? "\u2501" : " ");
    std::cout << kReset << kDim << "  " << str::formatTime(snapshot.positionSec) << " / "
              << str::formatTime(duration) << kReset << "   ";
    std::cout.flush();
    if (finalLine) std::cout << "\n";
}

void printMetrics(Controller& controller) {
    const EngineMetrics metrics = controller.engine().metrics();
    std::cout << kDim << "  engine: " << metrics.framesRendered << " frames rendered, "
              << metrics.decodedFrames << " decoded, underruns: " << metrics.underruns
              << ", DSP: " << str::formatDouble(metrics.renderCpuMsPerSecond, 2)
              << " ms/s of audio, buffer: "
              << str::formatDouble(metrics.bufferFill * 100.0, 0) << "%" << kReset << "\n";
}

int runPlay(const Options& options, bool playNow) {
    if (options.positional.empty()) {
        std::cout << tr("error.notFound") << ": <file|url>\n";
        return 2;
    }
    const double seconds = options.number("seconds", 0.0);
    Controller controller;
    std::string error;
    if (!controller.initialize(makeSink(options, seconds > 0 ? seconds : 600.0), &error)) {
        std::cout << tr("common.error") << ": " << error << "\n";
        return 1;
    }
    applyPlaybackFlags(controller, options);

    int added = 0;
    for (const std::string& input : options.positional) {
        std::string message;
        added += controller.add(input, playNow && added == 0, &message);
        if (!message.empty()) std::cout << kDim << "  " << message << kReset << "\n";
    }
    if (!playNow) {
        controller.persist();
        controller.shutdown();
        return added > 0 ? 0 : 1;
    }
    if (added == 0 && controller.currentTrack().path.empty()) {
        std::cout << tr("error.notFound") << "\n";
        controller.shutdown();
        return 1;
    }

    banner();
    const auto start = std::chrono::steady_clock::now();
    bool finished = false;
    controller.engine().onFinished([&] { finished = true; });
    while (!finished) {
        printStatus(controller);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        const double elapsed =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        if (seconds > 0 && elapsed * std::max(1.0, options.number("fast", 1.0)) >= seconds) break;
        if (controller.snapshot().state == PlaybackState::Stopped && elapsed > 1.0) break;
    }
    printStatus(controller, true);
    printMetrics(controller);
    controller.persist();
    controller.shutdown();
    return 0;
}

int runDemo(const Options& options) {
    const double seconds = options.number("seconds", 15.0);
    Controller controller;
    std::string error;
    if (!controller.initialize(makeSink(options, seconds + 2.0), &error)) {
        std::cout << tr("common.error") << ": " << error << "\n";
        return 1;
    }
    if (options.has("dir")) {
        std::string message;
        controller.add(options.value("dir"), false, &message);
    }
    applyPlaybackFlags(controller, options);
    if (!options.has("crossfade")) controller.setCrossfade(2.0);

    std::vector<Track> tracks = controller.library().tracks();
    if (tracks.empty()) {
        std::cout << tr("library.empty") << " — " << tr("library.emptyHint") << "\n";
        controller.shutdown();
        return 1;
    }
    std::sort(tracks.begin(), tracks.end(), [](const Track& a, const Track& b) {
        if (a.album != b.album) return a.album < b.album;
        return a.trackNo < b.trackNo;
    });

    banner();
    const LibraryStats stats = controller.library().stats();
    std::cout << kDim << "  "
              << tr("library.stats", {std::to_string(stats.trackCount),
                                     std::to_string(stats.albumCount),
                                     std::to_string(stats.artistCount)})
              << kReset << "\n\n";

    controller.playTracks(tracks, 0, &error);
    controller.setCallbacks([&] {
        Controller::Callbacks callbacks;
        callbacks.trackChanged = [](const Track& track) {
            std::string artist = track.displayArtist();
            if (artist.empty()) artist = tr("common.unknownArtist");
            std::cout << "\r" << kAccent << "  \u266b " << kReset << kBold << track.displayTitle()
                      << kReset << kDim << "  —  " << artist << "  [" << str::formatTime(track.durationSec)
                      << "]" << kReset << "                    \n";
        };
        return callbacks;
    }());

    const auto start = std::chrono::steady_clock::now();
    const double factor = std::max(1.0, options.number("fast", 1.0));
    bool sawPlayback = false;
    double lastPosition = -1.0;
    int idlePolls = 0;
    for (;;) {
        printStatus(controller);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        const PlayerSnapshot snapshot = controller.snapshot();
        if (snapshot.state == PlaybackState::Playing) sawPlayback = true;
        if (snapshot.positionSec == lastPosition) ++idlePolls; else idlePolls = 0;
        lastPosition = snapshot.positionSec;
        const double elapsed =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        if (elapsed * factor >= seconds) break;
        // Offline renders finish far faster than real time: stop when the
        // queue is exhausted instead of printing the same line forever.
        if (sawPlayback && (snapshot.state == PlaybackState::Stopped || idlePolls >= 4)) break;
    }
    printStatus(controller, true);
    std::cout << "\n";
    printMetrics(controller);
    controller.persist();
    controller.shutdown();
    return 0;
}

int runScan(const Options& options) {
    Controller controller;
    std::string error;
    if (!controller.initialize(makeNullSink(1.0), &error)) {
        std::cout << tr("common.error") << ": " << error << "\n";
        return 1;
    }
    std::vector<std::string> folders = options.positional;
    if (folders.empty()) folders = controller.config().settings().musicFolders;
    if (folders.empty()) {
        std::cout << tr("settings.folders") << ": —\n";
        controller.shutdown();
        return 1;
    }
    for (const std::string& folder : folders) {
        std::cout << kDim << "  " << tr("library.scanning") << " " << folder << kReset << "\n";
    }
    const ScanReport report = controller.library().scan(folders, !options.has("full"));
    controller.library().save();
    controller.config().settings().musicFolders = controller.library().roots();
    controller.config().save();
    std::cout << "  " << tr("library.scanDone") << ": +" << report.added << " / ~" << report.updated
              << " / -" << report.removed << " (" << report.filesSeen << " "
              << tr("library.tracks") << ", " << str::formatDouble(report.elapsedSec, 2) << "s)\n";
    controller.shutdown();
    return 0;
}

void printTrackRow(std::size_t index, const Track& track) {
    std::string artist = track.displayArtist();
    if (artist.empty()) artist = tr("common.unknownArtist");
    std::cout << kDim << "  " << (index < 9 ? " " : "") << index + 1 << ". " << kReset
              << str::ellipsize(track.displayTitle(), 34);
    for (std::size_t i = str::utf8Length(str::ellipsize(track.displayTitle(), 34)); i < 35; ++i) {
        std::cout << ' ';
    }
    std::cout << kDim << str::ellipsize(artist, 22) << kReset;
    for (std::size_t i = str::utf8Length(str::ellipsize(artist, 22)); i < 23; ++i) std::cout << ' ';
    std::cout << kDim << str::formatTime(track.durationSec) << kReset;
    if (track.favorite) std::cout << "  \u2605";
    std::cout << "\n";
}

int runList(const Options& options) {
    Config config;
    config.load();
    MediaLibrary library(config.libraryIndexPath());
    library.load();
    std::vector<Track> tracks = library.tracks();
    if (tracks.empty()) {
        std::cout << "  " << tr("library.empty") << " — " << tr("library.emptyHint") << "\n";
        return 0;
    }
    std::sort(tracks.begin(), tracks.end(), [](const Track& a, const Track& b) {
        if (a.artist != b.artist) return str::toLower(a.artist) < str::toLower(b.artist);
        if (a.album != b.album) return a.album < b.album;
        return a.trackNo < b.trackNo;
    });
    const std::size_t limit = static_cast<std::size_t>(options.number("limit", 30));
    banner();
    const LibraryStats stats = library.stats();
    std::cout << kDim << "  "
              << tr("library.stats", {std::to_string(stats.trackCount),
                                     std::to_string(stats.albumCount),
                                     std::to_string(stats.artistCount)})
              << "  ·  " << str::formatTime(stats.totalDurationSec) << "  ·  "
              << str::formatBytes(stats.totalBytes) << kReset << "\n\n";
    for (std::size_t i = 0; i < tracks.size() && i < limit; ++i) printTrackRow(i, tracks[i]);
    if (tracks.size() > limit) {
        std::cout << kDim << "  … " << (tracks.size() - limit) << " " << tr("common.of") << " "
                  << tracks.size() << kReset << "\n";
    }
    return 0;
}

int runSearch(const Options& options) {
    if (options.positional.empty()) return 2;
    Config config;
    config.load();
    MediaLibrary library(config.libraryIndexPath());
    library.load();
    const std::string query = str::join(options.positional, " ");
    const std::vector<Track> results = library.search(query, 25);
    std::cout << kBold << "  " << tr("library.search") << ": " << query << kReset << "\n\n";
    if (results.empty()) {
        std::cout << kDim << "  " << tr("library.noResults") << kReset << "\n";
        return 0;
    }
    for (std::size_t i = 0; i < results.size(); ++i) printTrackRow(i, results[i]);
    return 0;
}

int runAlbums() {
    Config config;
    config.load();
    MediaLibrary library(config.libraryIndexPath());
    library.load();
    const std::vector<AlbumInfo> albums = library.albums();
    std::cout << kBold << "  " << tr("nav.albums") << " (" << albums.size() << ")" << kReset << "\n\n";
    for (const AlbumInfo& album : albums) {
        std::cout << "  " << str::ellipsize(album.name, 30);
        for (std::size_t i = str::utf8Length(str::ellipsize(album.name, 30)); i < 31; ++i) {
            std::cout << ' ';
        }
        std::cout << kDim << str::ellipsize(album.artist, 22) << "  " << album.trackCount << " "
                  << tr("library.tracks") << "  " << str::formatTime(album.durationSec) << kReset
                  << "\n";
    }
    return 0;
}

int runArtists() {
    Config config;
    config.load();
    MediaLibrary library(config.libraryIndexPath());
    library.load();
    const std::vector<ArtistInfo> artists = library.artists();
    std::cout << kBold << "  " << tr("nav.artists") << " (" << artists.size() << ")" << kReset
              << "\n\n";
    for (const ArtistInfo& artist : artists) {
        std::cout << "  " << str::ellipsize(artist.name, 30);
        for (std::size_t i = str::utf8Length(str::ellipsize(artist.name, 30)); i < 31; ++i) {
            std::cout << ' ';
        }
        std::cout << kDim << artist.albumCount << " " << tr("nav.albums") << "  "
                  << artist.trackCount << " " << tr("library.tracks") << kReset << "\n";
    }
    return 0;
}

int runInfo(const Options& options) {
    if (options.positional.empty()) return 2;
    const std::string path = options.positional[0];
    Tags tags;
    std::string error;
    if (!TagReader::read(path, &tags, &error)) {
        std::cout << tr("common.error") << ": " << error << "\n";
        return 1;
    }
    auto row = [](const std::string& label, const std::string& value) {
        if (value.empty()) return;
        std::cout << kDim << "  " << label << kReset << "  " << value << "\n";
    };
    std::cout << kBold << "  " << str::fileName(path) << kReset << "\n\n";
    row("title     ", tags.title);
    row("artist    ", tags.artist);
    row("album     ", tags.album);
    row("albumArt. ", tags.albumArtist);
    row("genre     ", tags.genre);
    row("year      ", tags.year);
    if (tags.trackNo > 0) row("track     ", std::to_string(tags.trackNo));
    row("duration  ", str::formatTime(tags.durationSec));
    if (tags.sampleRate > 0) {
        row("format    ", std::to_string(tags.sampleRate) + " Hz · " +
                              std::to_string(tags.channels) + " ch · " +
                              std::to_string(tags.bitrateKbps) + " kbps");
    }
    row("cover     ", tags.hasCover ? tags.coverMime : std::string());
    row("parser    ", tags.parser);
    return 0;
}

int runWaveform(const Options& options) {
    if (options.positional.empty()) return 2;
    const int buckets = static_cast<int>(options.number("width", 72));
    Config config;
    config.load();
    DecoderOptions decoderOptions;
    decoderOptions.ffmpegPath = config.settings().ffmpegPath;
    decoderOptions.ffprobePath = config.settings().ffprobePath;

    Waveform waveform;
    std::string error;
    if (!computeWaveform(options.positional[0], buckets, &waveform, decoderOptions, &error)) {
        std::cout << tr("common.error") << ": " << error << "\n";
        return 1;
    }
    const char* blocks[] = {" ", "\u2581", "\u2582", "\u2583", "\u2584",
                            "\u2585", "\u2586", "\u2587", "\u2588"};
    std::cout << "  " << kAccent;
    for (const float peak : waveform.peaks) {
        int level = static_cast<int>(peak * 8.0f + 0.5f);
        if (level < 0) level = 0;
        if (level > 8) level = 8;
        std::cout << blocks[level];
    }
    std::cout << kReset << "\n" << kDim << "  " << str::formatTime(waveform.durationSec) << "  ·  "
              << waveform.peaks.size() << " buckets" << kReset << "\n";
    return 0;
}

int runEq(const Options& options) {
    const std::vector<std::string> presets = Equalizer::presetNames();
    if (options.positional.empty()) {
        std::cout << kBold << "  " << tr("eq.title") << kReset << "\n\n";
        for (const std::string& preset : presets) {
            const std::vector<float> gains = Equalizer::presetGains(preset);
            std::cout << "  " << str::ellipsize(tr("eq.preset." + preset), 16);
            for (std::size_t i = str::utf8Length(tr("eq.preset." + preset)); i < 17; ++i) {
                std::cout << ' ';
            }
            std::cout << kDim;
            for (const float gain : gains) {
                std::cout << (gain > 0 ? "+" : "") << str::formatDouble(gain, 0) << " ";
            }
            std::cout << kReset << "\n";
        }
        std::cout << "\n" << kDim << "  " << tr("eq.preamp") << ": ±12 dB · "
                  << Equalizer::kBands << " bands" << kReset << "\n";
        return 0;
    }
    Config config;
    config.load();
    Equalizer equalizer;
    if (!equalizer.applyPreset(options.positional[0])) {
        std::cout << tr("common.error") << ": " << options.positional[0] << "\n";
        return 1;
    }
    config.settings().equalizerEnabled = true;
    config.settings().equalizerPreset = options.positional[0];
    config.settings().equalizerGains = equalizer.gains();
    config.save();
    std::cout << "  " << tr("eq.preset") << ": " << tr("eq.preset." + options.positional[0]) << "\n";
    return 0;
}

int runDownload(const Options& options) {
    if (options.positional.empty()) return 2;
    Config config;
    config.load();
    Config::ensureDir(config.settings().downloadDir);
    Downloader downloader(config.settings().downloadDir, config.settings().ytdlpPath,
                          config.settings().ffmpegPath);
    if (!downloader.hasYtDlp()) {
        std::cout << "  " << tr("download.needYtDlp") << "\n";
    }
    downloader.setOnProgress([](const DownloadJob& job) {
        std::cout << "\r  " << kAccent << str::formatDouble(job.progress * 100.0, 1) << "%" << kReset
                  << kDim << "  " << str::ellipsize(job.title, 40) << "  "
                  << str::formatDouble(job.speedKbps / 1024.0, 2) << " MB/s" << kReset << "      ";
        std::cout.flush();
    });
    downloader.start(1);
    for (const std::string& url : options.positional) downloader.enqueue(url);
    downloader.waitAll();
    std::cout << "\n";

    MediaLibrary library(config.libraryIndexPath());
    library.load();
    int imported = 0;
    for (const DownloadJob& job : downloader.jobs()) {
        if (job.state == DownloadState::Completed) {
            Track track;
            if (library.addDownloaded(job.outputPath, job.url, &track)) {
                ++imported;
                std::cout << "  " << tr("download.completed") << ": " << track.displayTitle() << "\n";
            }
        } else {
            std::cout << "  " << tr("download.failed") << ": " << job.error << "\n";
        }
    }
    library.save();
    downloader.stop();
    return imported > 0 ? 0 : 1;
}

int runBench(const Options& options) {
    if (options.positional.empty()) return 2;
    Config config;
    config.load();
    DecoderOptions decoderOptions;
    decoderOptions.sampleRate = config.settings().sampleRate;
    decoderOptions.channels = config.settings().channels;
    decoderOptions.ffmpegPath = config.settings().ffmpegPath;

    std::string error;
    auto decoder = makeDecoder(options.positional[0], decoderOptions, &error);
    if (!decoder) {
        std::cout << tr("common.error") << ": " << error << "\n";
        return 1;
    }
    const auto start = std::chrono::steady_clock::now();
    std::vector<float> buffer(4096 * static_cast<std::size_t>(decoder->channels()));
    std::uint64_t frames = 0;
    std::size_t got = 0;
    while ((got = decoder->read(buffer.data(), 4096)) > 0) frames += got;
    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    const double audioSeconds = static_cast<double>(frames) / decoder->sampleRate();
    std::cout << kBold << "  " << decoder->name() << kReset << "\n"
              << kDim << "  decoded " << str::formatTime(audioSeconds) << " in "
              << str::formatDouble(elapsed, 3) << " s  →  "
              << str::formatDouble(elapsed > 0 ? audioSeconds / elapsed : 0.0, 1)
              << "x realtime" << kReset << "\n";
    return 0;
}

int runStats() {
    Config config;
    config.load();
    MediaLibrary library(config.libraryIndexPath());
    library.load();
    const LibraryStats stats = library.stats();
    banner();
    std::cout << "\n" << kBold << "  " << tr("library.title") << kReset << "\n";
    std::cout << kDim << "  " << tr("library.tracks") << ": " << kReset << stats.trackCount << "\n"
              << kDim << "  " << tr("nav.albums") << ": " << kReset << stats.albumCount << "\n"
              << kDim << "  " << tr("nav.artists") << ": " << kReset << stats.artistCount << "\n"
              << kDim << "  " << tr("add.stream") << ": " << kReset << stats.streamCount << "\n"
              << kDim << "  " << tr("time.total") << ": " << kReset
              << str::formatTime(stats.totalDurationSec) << "\n"
              << kDim << "  size: " << kReset << str::formatBytes(stats.totalBytes) << "\n";
    std::cout << "\n" << kBold << "  " << tr("settings.title") << kReset << "\n";
    std::cout << kDim << "  config: " << kReset << config.path() << "\n"
              << kDim << "  library: " << kReset << config.libraryIndexPath() << "\n"
              << kDim << "  downloads: " << kReset << config.settings().downloadDir << "\n"
              << kDim << "  " << tr("settings.language") << ": " << kReset
              << I18n::languageName(I18n::instance().language()) << "\n"
              << kDim << "  " << tr("settings.folders") << ": " << kReset
              << str::join(config.settings().musicFolders, ", ") << "\n";
    return 0;
}

int runDoctor() {
    Config config;
    config.load();
    banner();
    std::cout << "\n";
    auto check = [](const std::string& label, bool ok, const std::string& detail) {
        std::cout << "  " << (ok ? "\u2713" : "\u2717") << "  " << label;
        for (std::size_t i = label.size(); i < 14; ++i) std::cout << ' ';
        std::cout << kDim << (ok ? tr("settings.installed") : tr("settings.missing"));
        if (!detail.empty()) std::cout << "  " << detail;
        std::cout << kReset << "\n";
    };
    const bool ffmpeg = hasFfmpeg(config.settings().ffmpegPath);
    check("ffmpeg", ffmpeg, ffmpeg ? "" : tr("download.needFfmpeg"));
    check("ffprobe", Process::exists(config.settings().ffprobePath), "");
    const bool ytdlp = Process::exists(config.settings().ytdlpPath);
    check("yt-dlp", ytdlp, ytdlp ? "" : tr("download.needYtDlp"));
    std::cout << "\n" << kDim << "  " << tr("add.hint") << kReset << "\n";
    std::cout << kDim << "  WAV/native decoder: " << tr("settings.installed") << kReset << "\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    const Options options = parseOptions(args);

    Config bootstrap;
    bootstrap.load();
    I18n::instance().setLanguage(options.value("lang", bootstrap.settings().language));
    Log::setLevel(options.has("verbose") ? LogLevel::Debug
                                         : (options.has("quiet") ? LogLevel::Error : LogLevel::Warn));

    if (args.empty()) {
        printHelp();
        return 0;
    }
    const std::string command = str::toLower(args[0]);
    Options rest = options;
    if (!rest.positional.empty()) rest.positional.erase(rest.positional.begin());

    if (command == "help" || command == "--help" || command == "-h") {
        printHelp();
        return 0;
    }
    if (command == "version" || command == "--version") {
        std::cout << "Aurora Player " << versionString() << "\n";
        return 0;
    }
    if (command == "play") return runPlay(rest, true);
    if (command == "add") return runPlay(rest, false);
    if (command == "demo") return runDemo(rest);
    if (command == "scan") return runScan(rest);
    if (command == "list" || command == "ls") return runList(rest);
    if (command == "search" || command == "find") return runSearch(rest);
    if (command == "albums") return runAlbums();
    if (command == "artists") return runArtists();
    if (command == "info") return runInfo(rest);
    if (command == "waveform") return runWaveform(rest);
    if (command == "eq") return runEq(rest);
    if (command == "download" || command == "dl") return runDownload(rest);
    if (command == "bench") return runBench(rest);
    if (command == "stats") return runStats();
    if (command == "doctor") return runDoctor();
    if (command == "lang") {
        if (rest.positional.empty()) {
            std::cout << I18n::instance().language() << "\n";
            return 0;
        }
        Config config;
        config.load();
        if (!I18n::instance().setLanguage(rest.positional[0])) {
            std::cout << tr("common.error") << ": " << rest.positional[0] << "\n";
            return 1;
        }
        config.settings().language = rest.positional[0];
        config.save();
        std::cout << "  " << tr("settings.language") << ": "
                  << I18n::languageName(rest.positional[0]) << "\n";
        return 0;
    }

    std::cout << tr("common.error") << ": " << command << "\n\n";
    printHelp();
    return 2;
}
