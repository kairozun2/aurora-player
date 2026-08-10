// Aurora Player - core test suite (no external test framework required).
//
// Everything runs headless: audio is rendered through the null sink at many
// times real time, so the whole suite finishes in a couple of seconds and can
// run in CI on Linux, macOS and Windows.
#include "aurora/Analysis.hpp"
#include "aurora/AudioEngine.hpp"
#include "aurora/Config.hpp"
#include "aurora/Controller.hpp"
#include "aurora/Decoder.hpp"
#include "aurora/Downloader.hpp"
#include "aurora/Equalizer.hpp"
#include "aurora/I18n.hpp"
#include "aurora/Json.hpp"
#include "aurora/Log.hpp"
#include "aurora/Lyrics.hpp"
#include "aurora/MediaLibrary.hpp"
#include "aurora/Playlist.hpp"
#include "aurora/Process.hpp"
#include "aurora/RingBuffer.hpp"
#include "aurora/Sink.hpp"
#include "aurora/Strings.hpp"
#include "aurora/TagReader.hpp"
#include "aurora/Types.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace aurora;
namespace fs = std::filesystem;

namespace {

int gChecks = 0;
int gFailures = 0;
std::string gSection;

void section(const std::string& name) {
    gSection = name;
    std::cout << "\n\033[1m" << name << "\033[0m\n";
}

void check(bool ok, const std::string& what, int line) {
    ++gChecks;
    if (ok) {
        std::cout << "  \033[32m\u2713\033[0m " << what << "\n";
    } else {
        ++gFailures;
        std::cout << "  \033[31m\u2717 " << what << "\033[0m  (" << gSection << ":" << line << ")\n";
    }
}

#define CHECK(cond) check((cond), #cond, __LINE__)
#define CHECK_MSG(cond, msg) check((cond), (msg), __LINE__)
#define CHECK_NEAR(a, b, eps) \
    check(std::fabs((double)(a) - (double)(b)) <= (eps), \
          std::string(#a) + " \u2248 " + #b, __LINE__)

void setEnvVar(const char* key, const std::string& value) {
#ifdef _WIN32
    _putenv_s(key, value.c_str());
#else
    setenv(key, value.c_str(), 1);
#endif
}

template <typename Predicate>
bool waitFor(Predicate predicate, int milliseconds) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return predicate();
}

void writeLe32(std::ofstream& out, std::uint32_t value) {
    unsigned char bytes[4] = {static_cast<unsigned char>(value & 0xFF),
                              static_cast<unsigned char>((value >> 8) & 0xFF),
                              static_cast<unsigned char>((value >> 16) & 0xFF),
                              static_cast<unsigned char>((value >> 24) & 0xFF)};
    out.write(reinterpret_cast<const char*>(bytes), 4);
}

void writeLe16(std::ofstream& out, std::uint16_t value) {
    unsigned char bytes[2] = {static_cast<unsigned char>(value & 0xFF),
                              static_cast<unsigned char>((value >> 8) & 0xFF)};
    out.write(reinterpret_cast<const char*>(bytes), 2);
}

/// Writes a PCM16 WAV file with a sine tone (or silence when freq <= 0).
bool writeSineWav(const std::string& path,
                  double seconds,
                  double freq,
                  int sampleRate = 44100,
                  int channels = 1,
                  double silenceRatio = 0.0) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    const std::uint32_t frames = static_cast<std::uint32_t>(seconds * sampleRate);
    const std::uint32_t dataBytes = frames * static_cast<std::uint32_t>(channels) * 2u;
    out.write("RIFF", 4);
    writeLe32(out, 36 + dataBytes);
    out.write("WAVEfmt ", 8);
    writeLe32(out, 16);
    writeLe16(out, 1);
    writeLe16(out, static_cast<std::uint16_t>(channels));
    writeLe32(out, static_cast<std::uint32_t>(sampleRate));
    writeLe32(out, static_cast<std::uint32_t>(sampleRate * channels * 2));
    writeLe16(out, static_cast<std::uint16_t>(channels * 2));
    writeLe16(out, 16);
    out.write("data", 4);
    writeLe32(out, dataBytes);
    const std::uint32_t silentFrames = static_cast<std::uint32_t>(frames * silenceRatio);
    for (std::uint32_t i = 0; i < frames; ++i) {
        double sample = 0.0;
        if (freq > 0.0 && i >= silentFrames) {
            sample = 0.7 * std::sin(2.0 * 3.14159265358979 * freq * i / sampleRate);
        }
        const std::int16_t pcm = static_cast<std::int16_t>(sample * 32767.0);
        for (int c = 0; c < channels; ++c) writeLe16(out, static_cast<std::uint16_t>(pcm));
    }
    return true;
}

double rmsOf(const std::vector<float>& samples) {
    if (samples.empty()) return 0.0;
    double sum = 0.0;
    for (const float sample : samples) sum += static_cast<double>(sample) * sample;
    return std::sqrt(sum / samples.size());
}

// ---------------------------------------------------------------- strings ---
void testStrings() {
    section("strings");
    CHECK(str::formatTime(0.0) == "0:00");
    CHECK(str::formatTime(65.4) == "1:05");
    CHECK(str::formatTime(3725.0) == "1:02:05");
    CHECK(str::toLower("\xD0\x9A\xD0\x98\xD0\x9D\xD0\x9E") == "\xD0\xBA\xD0\xB8\xD0\xBD\xD0\xBE");
    CHECK(str::trim("  hi \n") == "hi");
    CHECK(str::split("a,b,,c", ',').size() == 3);
    CHECK(str::split("a,b,,c", ',', true).size() == 4);
    CHECK(str::iContains("The Weeknd", "weeknd"));
    CHECK(str::replaceAll("a-b-c", "-", "+") == "a+b+c");
    CHECK(str::normalize("  Hello,   World! ") == "hello world");
    CHECK(!str::translit("\xD0\x9A\xD0\xB8\xD0\xBD\xD0\xBE").empty());
    CHECK(str::translit("\xD0\x9A\xD0\xB8\xD0\xBD\xD0\xBE") == "kino");
    CHECK(str::utf8Length("\xD0\x9C\xD1\x83\xD0\xB7\xD1\x8B\xD0\xBA\xD0\xB0") == 6);
    CHECK(str::utf8Length(str::ellipsize("abcdefghij", 5)) <= 6);
    CHECK(str::hashId("x").size() == 16);
    CHECK(str::hash64("a") != str::hash64("b"));
    CHECK(str::extension("/x/y/Track.MP3") == "mp3");
    CHECK(str::stem("/x/y/Track.mp3") == "Track");
    CHECK(str::fileName("/x/y/Track.mp3") == "Track.mp3");
    CHECK(str::joinPath("/x/", "y.mp3") == "/x/y.mp3");
    CHECK(str::sanitizeFileName("a/b:c*?.mp3").find('/') == std::string::npos);
    CHECK(str::isUrl("https://example.com/a.mp3"));
    CHECK(!str::isUrl("/home/user/a.mp3"));
    CHECK(str::formatBytes(1536) == "1.5 KB");
    CHECK(str::formatDouble(3.14159, 2) == "3.14");
}

// ------------------------------------------------------------------- json ---
void testJson() {
    section("json");
    std::string error;
    const Json parsed = Json::parse(
        "{\"a\":1,\"b\":[1,2,3],\"c\":{\"d\":\"\\u041c\\u0443\\u0437\"},\"e\":true,\"f\":null}",
        &error);
    CHECK_MSG(error.empty(), "parse without error");
    CHECK(parsed.isObject());
    CHECK(parsed["a"].asInt(0) == 1);
    CHECK(parsed["b"].size() == 3);
    CHECK(parsed["b"].at(2).asInt(0) == 3);
    CHECK(parsed["c"]["d"].asString("") == "\xD0\x9C\xD1\x83\xD0\xB7");
    CHECK(parsed["e"].asBool(false));
    CHECK(parsed.contains("f"));

    Json object = Json::object();
    object.set("title", Json("\xD0\x9F\xD0\xB5\xD1\x81\xD0\xBD\xD1\x8F"));
    object.set("volume", Json(0.85));
    Json array = Json::array();
    array.push(Json("a"));
    array.push(Json("b"));
    object.set("tags", array);
    const std::string dumped = object.dump(0);
    const Json again = Json::parse(dumped, &error);
    CHECK_MSG(error.empty(), "roundtrip parse");
    CHECK(again["title"].asString("") == "\xD0\x9F\xD0\xB5\xD1\x81\xD0\xBD\xD1\x8F");
    CHECK_NEAR(again["volume"].asDouble(0.0), 0.85, 1e-9);
    CHECK(again["tags"].size() == 2);
    CHECK(Json::parse("{bad json", &error).isObject() == false || !error.empty());
}

// -------------------------------------------------------------- ringbuffer --
void testRingBuffer() {
    section("ring buffer");
    RingBuffer ring(8);
    CHECK(ring.capacity() == 8);
    CHECK(ring.available() == 0);
    const float input[6] = {1, 2, 3, 4, 5, 6};
    CHECK(ring.write(input, 6) == 6);
    CHECK(ring.available() == 6);
    CHECK(ring.space() == 2);
    float output[4] = {0, 0, 0, 0};
    CHECK(ring.read(output, 4) == 4);
    CHECK(output[0] == 1.0f && output[3] == 4.0f);
    CHECK(ring.write(input, 5) == 5);  // wraps around
    CHECK(ring.available() == 7);
    float rest[8] = {0};
    CHECK(ring.read(rest, 8) == 7);
    CHECK(rest[0] == 5.0f && rest[2] == 1.0f);
    CHECK(ring.available() == 0);
    CHECK_NEAR(ring.fill(), 0.0, 1e-9);
    ring.clear();
    CHECK(ring.read(rest, 1) == 0);
}

// --------------------------------------------------------------- equalizer --
void testEqualizer() {
    section("equalizer");
    CHECK(Equalizer::kBands == 10);
    CHECK(Equalizer::presetNames().size() >= 10);
    CHECK(Equalizer::bandFrequencies().size() == static_cast<std::size_t>(Equalizer::kBands));

    const int sampleRate = 48000;
    const int frames = 4800;
    std::vector<float> tone(frames * 2);
    for (int i = 0; i < frames; ++i) {
        const float sample =
            static_cast<float>(0.5 * std::sin(2.0 * 3.14159265358979 * 60.0 * i / sampleRate));
        tone[i * 2] = sample;
        tone[i * 2 + 1] = sample;
    }

    Equalizer flat;
    flat.setSampleRate(sampleRate);
    flat.setEnabled(true);
    flat.applyPreset("flat");
    std::vector<float> flatOut = tone;
    flat.process(flatOut.data(), frames, 2);
    CHECK_NEAR(rmsOf(flatOut), rmsOf(tone), 0.02);

    Equalizer bass;
    bass.setSampleRate(sampleRate);
    bass.setEnabled(true);
    CHECK(bass.applyPreset("bass"));
    CHECK(bass.currentPreset() == "bass");
    std::vector<float> bassOut = tone;
    bass.process(bassOut.data(), frames, 2);
    CHECK_MSG(rmsOf(bassOut) > rmsOf(tone) * 1.1, "bass preset boosts 60 Hz");

    Equalizer disabled;
    disabled.setSampleRate(sampleRate);
    disabled.setEnabled(false);
    disabled.applyPreset("bass");
    std::vector<float> untouched = tone;
    disabled.process(untouched.data(), frames, 2);
    CHECK(untouched == tone);

    Equalizer clamped;
    clamped.setBandGain(0, 99.0f);
    CHECK(clamped.bandGain(0) <= 12.0f);
    clamped.setPreampDb(-99.0f);
    CHECK(clamped.preampDb() >= -12.0f);
    CHECK(clamped.gains().size() == static_cast<std::size_t>(Equalizer::kBands));

    // Denormal / NaN safety: garbage in must not propagate NaN out.
    Equalizer guard;
    guard.setEnabled(true);
    guard.applyPreset("rock");
    std::vector<float> dirty(256, std::nanf(""));
    guard.process(dirty.data(), 128, 2);
    bool finite = true;
    for (const float sample : dirty) {
        if (!std::isfinite(sample)) finite = false;
    }
    CHECK_MSG(finite, "NaN input is sanitised");
}

// ----------------------------------------------------------------- decoder --
void testDecoder(const fs::path& dir) {
    section("decoder");
    const std::string mono = (dir / "tone_mono.wav").string();
    CHECK(writeSineWav(mono, 1.0, 440.0, 44100, 1));
    CHECK(isSupportedAudioFile("a.mp3") && isSupportedAudioFile("b.FLAC"));
    CHECK(!isSupportedAudioFile("notes.txt"));

    std::string error;
    auto raw = makeWavDecoder(mono, &error);
    CHECK_MSG(raw != nullptr, "wav decoder opens the file");
    if (raw) {
        CHECK(raw->sampleRate() == 44100);
        CHECK(raw->channels() == 1);
        CHECK_NEAR(raw->duration(), 1.0, 0.01);
        std::vector<float> buffer(1024);
        const std::size_t got = raw->read(buffer.data(), 1024);
        CHECK(got == 1024);
        CHECK_MSG(rmsOf(buffer) > 0.3, "decoded tone has signal");
        CHECK(raw->seek(0.5));
        CHECK(!raw->eof());
        CHECK(raw->name().find("wav") != std::string::npos ||
              raw->name().find("WAV") != std::string::npos);
    }

    // Converter: 44100/mono -> 48000/stereo, plus speed control.
    auto source = makeWavDecoder(mono, &error);
    auto converted = makeConverter(std::move(source), 48000, 2);
    CHECK_MSG(converted != nullptr, "converter wraps the decoder");
    if (converted) {
        CHECK(converted->sampleRate() == 48000);
        CHECK(converted->channels() == 2);
        std::size_t frames = 0;
        std::vector<float> buffer(2048 * 2);
        std::size_t got = 0;
        while ((got = converted->read(buffer.data(), 2048)) > 0) frames += got;
        CHECK_NEAR(static_cast<double>(frames) / 48000.0, 1.0, 0.05);
    }

    auto fast = makeDecoder(mono, DecoderOptions(), &error);
    CHECK_MSG(fast != nullptr, "makeDecoder picks a backend");
    if (fast) {
        fast->setSpeed(2.0);
        std::size_t frames = 0;
        std::vector<float> buffer(2048 * 2);
        std::size_t got = 0;
        while ((got = fast->read(buffer.data(), 2048)) > 0) frames += got;
        CHECK_MSG(std::fabs(static_cast<double>(frames) / 48000.0 - 0.5) < 0.06,
                  "2x speed halves the output length");
    }

    auto missing = makeDecoder((dir / "nope.wav").string(), DecoderOptions(), &error);
    CHECK_MSG(missing == nullptr && !error.empty(), "missing file reports an error");
}

// -------------------------------------------------------------------- tags --
void testTags(const fs::path& dir) {
    section("tags");
    const std::string path = (dir / "Kino - Gruppa krovi.wav").string();
    CHECK(writeSineWav(path, 0.5, 220.0, 44100, 2));
    Tags tags;
    std::string error;
    CHECK(TagReader::read(path, &tags, &error));
    CHECK(tags.sampleRate == 44100);
    CHECK(tags.channels == 2);
    CHECK_NEAR(tags.durationSec, 0.5, 0.05);
    CHECK(!tags.parser.empty());

    const Tags guessed = TagReader::fromFileName("/music/Zemfira - Iskala.mp3");
    CHECK(guessed.artist == "Zemfira");
    CHECK(guessed.title == "Iskala");
    const Tags plain = TagReader::fromFileName("/music/03 - Sailing.flac");
    CHECK(plain.title == "Sailing");
    CHECK(plain.trackNo == 3);
}

// ------------------------------------------------------------------ lyrics --
void testLyrics(const fs::path& dir) {
    section("lyrics");
    const std::string path = (dir / "song.lrc").string();
    {
        std::ofstream out(path);
        out << "[ti:Blinding Lights]\n[ar:The Weeknd]\n[offset:0]\n"
            << "[00:01.00]I said, ooh\n[00:03.50]I'm blinded by the lights\n"
            << "[00:07.00]\xD0\x9F\xD1\x80\xD0\xB8\xD0\xBF\xD0\xB5\xD0\xB2\n";
    }
    Lyrics lyrics;
    CHECK(lyrics.loadFile(path));
    CHECK(!lyrics.empty());
    CHECK(lyrics.synced());
    CHECK(lyrics.lines().size() == 3);
    CHECK(lyrics.title() == "Blinding Lights");
    CHECK(lyrics.artist() == "The Weeknd");
    CHECK(lyrics.indexAt(0.5) < 0);
    CHECK(lyrics.indexAt(2.0) == 0);
    CHECK(lyrics.indexAt(4.0) == 1);
    CHECK(lyrics.textAt(8.0) == "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xBF\xD0\xB5\xD0\xB2");
    CHECK(!lyrics.plainText().empty());

    Lyrics plain;
    plain.loadText("just a line\nand another");
    CHECK(!plain.empty());
    CHECK(!plain.synced());
    CHECK(plain.lines().size() == 2);
}

// -------------------------------------------------------------------- i18n --
void testI18n() {
    section("i18n");
    I18n& i18n = I18n::instance();
    CHECK(i18n.availableLanguages().size() >= 2);
    CHECK(i18n.setLanguage("ru"));
    CHECK(i18n.language() == "ru");
    CHECK(tr("player.play") == "\xD0\x98\xD0\xB3\xD1\x80\xD0\xB0\xD1\x82\xD1\x8C");
    CHECK(tr("nav.library") == "\xD0\x9C\xD0\xB5\xD0\xB4\xD0\xB8\xD0\xB0\xD1\x82\xD0\xB5\xD0\xBA\xD0\xB0");
    CHECK(tr("common.tracks", {"7"}).find("7") != std::string::npos);
    CHECK(i18n.setLanguage("en"));
    CHECK(tr("player.play") == "Play");
    CHECK(tr("common.tracks", {"7"}) == "7 tracks");
    CHECK(tr("this.key.does.not.exist") == "this.key.does.not.exist");
    CHECK(!I18n::languageName("ru").empty());
    CHECK(!I18n::detectSystemLanguage().empty());
    i18n.define("en", "custom.key", "Custom");
    CHECK(tr("custom.key") == "Custom");
    CHECK(!i18n.setLanguage("klingon"));
}

// ------------------------------------------------------------------ config --
void testConfig(const fs::path& dir) {
    section("config");
    const std::string path = (dir / "settings.json").string();
    {
        Config config(path);
        config.settings().volume = 0.42f;
        config.settings().language = "ru";
        config.settings().crossfadeSec = 3.5;
        config.settings().repeat = RepeatMode::All;
        config.settings().shuffle = true;
        config.settings().equalizerEnabled = true;
        config.settings().equalizerPreset = "night";
        config.settings().equalizerGains = std::vector<float>(10, 2.0f);
        config.settings().musicFolders = {dir.string()};
        CHECK(config.save());
    }
    Config reloaded(path);
    CHECK(reloaded.load());
    CHECK_NEAR(reloaded.settings().volume, 0.42, 1e-6);
    CHECK(reloaded.settings().language == "ru");
    CHECK_NEAR(reloaded.settings().crossfadeSec, 3.5, 1e-9);
    CHECK(reloaded.settings().repeat == RepeatMode::All);
    CHECK(reloaded.settings().shuffle);
    CHECK(reloaded.settings().equalizerEnabled);
    CHECK(reloaded.settings().equalizerPreset == "night");
    CHECK(reloaded.settings().equalizerGains.size() == 10);
    CHECK(reloaded.settings().musicFolders.size() == 1);
    CHECK(!Config::dataDir().empty() && !Config::cacheDir().empty());
    CHECK(!reloaded.libraryIndexPath().empty());
    CHECK(!reloaded.playlistsPath().empty());
}

// ----------------------------------------------------------------- library --
void testLibrary(const fs::path& root) {
    section("media library");
    const fs::path musicDir = root / "music";
    fs::create_directories(musicDir / "album");
    CHECK(writeSineWav((musicDir / "album" / "01 - Sailing.wav").string(), 0.4, 300.0));
    CHECK(writeSineWav((musicDir / "album" / "02 - Ride Like the Wind.wav").string(), 0.4, 350.0));
    CHECK(writeSineWav((musicDir / "\xD0\x9A\xD0\xB8\xD0\xBD\xD0\xBE - \xD0\x97\xD0\xB2\xD0\xB5\xD0\xB7\xD0\xB4\xD0\xB0.wav").string(), 0.4, 400.0));
    { std::ofstream ignored((musicDir / "cover.jpg").string()); ignored << "not audio"; }

    MediaLibrary library((root / "library.json").string());
    const ScanReport report = library.scan({musicDir.string()}, true);
    CHECK(report.added == 3);
    CHECK(report.filesSeen >= 3);
    CHECK(!report.cancelled);
    CHECK(library.size() == 3);

    const ScanReport again = library.scan({musicDir.string()}, true);
    CHECK_MSG(again.skipped == 3 && again.added == 0, "incremental scan skips unchanged files");

    CHECK(library.save());
    MediaLibrary reloaded((root / "library.json").string());
    CHECK(reloaded.load());
    CHECK(reloaded.size() == 3);

    const std::vector<Track> hits = library.search("sailing");
    CHECK(!hits.empty());
    CHECK(hits.front().title.find("Sailing") != std::string::npos);
    CHECK(!library.search("\xD0\x9A\xD0\xB8\xD0\xBD\xD0\xBE").empty());
    CHECK_MSG(!library.search("kino").empty(), "transliterated search finds Cyrillic titles");
    CHECK(library.search("zzzznothing").empty());

    const std::vector<Track> all = library.tracks();
    CHECK(all.size() == 3);
    const std::string firstId = all.front().id;
    Track fetched;
    CHECK(library.track(firstId, &fetched));
    CHECK(library.trackByPath(all.front().path, &fetched));
    CHECK(library.toggleFavorite(firstId));
    CHECK(library.favorites().size() == 1);
    CHECK(library.setRating(firstId, 5));
    library.markPlayed(firstId);
    CHECK(library.track(firstId, &fetched) && fetched.playCount == 1);
    CHECK(library.mostPlayed(5).front().id == firstId);
    CHECK(library.recentlyPlayed(5).front().id == firstId);
    CHECK(library.recentlyAdded(5).size() == 3);
    CHECK(library.neverPlayed(5).size() == 2);
    CHECK(library.shuffled(3).size() == 3);

    const LibraryStats stats = library.stats();
    CHECK(stats.trackCount == 3);
    CHECK(stats.totalDurationSec > 1.0);
    CHECK(stats.totalBytes > 0);
    CHECK(!library.albums().empty());
    CHECK(!library.artists().empty());

    Track stream;
    CHECK(library.addStream("https://example.com/radio.mp3", "Radio", &stream));
    CHECK(stream.isStream());
    CHECK(library.stats().streamCount == 1);
    CHECK(library.remove(stream.id));
    CHECK(library.size() == 3);
    CHECK(!library.addFile((root / "missing.wav").string()));
}

// ------------------------------------------------------------------- queue --
void testQueue() {
    section("play queue");
    auto make = [](const std::string& name) {
        Track track;
        track.id = name;
        track.path = "/music/" + name + ".wav";
        track.title = name;
        return track;
    };
    std::vector<Track> tracks = {make("a"), make("b"), make("c")};

    PlayQueue queue;
    queue.setTracks(tracks, 0);
    CHECK(queue.size() == 3);
    CHECK(!queue.empty());
    Track current;
    CHECK(queue.current(&current) && current.id == "a");
    CHECK(queue.peekNext(&current) && current.id == "b");

    Track out;
    CHECK(queue.next(&out) && out.id == "b");
    CHECK(queue.next(&out) && out.id == "c");
    CHECK_MSG(!queue.next(&out), "repeat off stops at the end");

    queue.setRepeat(RepeatMode::All);
    CHECK(queue.next(&out) && out.id == "a");
    queue.setRepeat(RepeatMode::One);
    CHECK_MSG(queue.next(&out, false) && out.id == "a", "repeat one replays automatically");
    CHECK_MSG(queue.next(&out, true) && out.id == "b", "manual next ignores repeat one");
    queue.setRepeat(RepeatMode::Off);
    CHECK(queue.previous(&out) && out.id == "a");

    queue.insertNext(make("x"));
    CHECK(queue.peekNext(&out) && out.id == "x");
    CHECK(queue.size() == 4);
    CHECK(queue.upcoming(2).size() == 2);
    CHECK(queue.move(3, 1));
    CHECK(queue.removeAt(1));
    CHECK(queue.size() == 3);
    CHECK(queue.trackAt(0, &out) && out.id == "a");
    CHECK(!queue.trackAt(99, &out));

    // Shuffle must visit every track exactly once before repeating.
    PlayQueue shuffled;
    shuffled.setTracks(tracks, 0);
    shuffled.setShuffle(true);
    CHECK(shuffled.shuffle());
    CHECK(shuffled.current(&current) && current.id == "a");
    std::vector<std::string> visited{current.id};
    while (shuffled.next(&out)) visited.push_back(out.id);
    CHECK_MSG(visited.size() == 3, "shuffle covers the queue once");
    std::sort(visited.begin(), visited.end());
    CHECK(visited[0] == "a" && visited[1] == "b" && visited[2] == "c");

    queue.clear();
    CHECK(queue.empty());
    CHECK(!queue.current(&out));
}

// --------------------------------------------------------------- playlists --
void testPlaylists(const fs::path& dir) {
    section("playlists");
    const std::string path = (dir / "playlists.json").string();
    std::string id;
    {
        PlaylistStore store(path);
        id = store.create("\xD0\x92\xD0\xB5\xD1\x87\xD0\xB5\xD1\x80");  // "Vecher"
        CHECK(!id.empty());
        CHECK(store.size() == 1);
        CHECK(store.addTracks(id, {"t1", "t2", "t3"}));
        CHECK(store.removeTrack(id, "t2"));
        CHECK(store.moveTrack(id, 1, 0));
        CHECK(store.rename(id, "Evening"));
        CHECK(store.save());
    }
    PlaylistStore reloaded(path);
    CHECK(reloaded.load());
    Playlist playlist;
    CHECK(reloaded.get(id, &playlist));
    CHECK(playlist.name == "Evening");
    CHECK(playlist.trackIds.size() == 2);
    CHECK(playlist.trackIds[0] == "t3");
    CHECK(reloaded.findByName("Evening", &playlist));
    CHECK(reloaded.all().size() == 1);
    CHECK(reloaded.remove(id));
    CHECK(reloaded.size() == 0);
}

// -------------------------------------------------------------- m3u export --
void testM3u(const fs::path& dir) {
    section("m3u");
    const std::string m3u = (dir / "list.m3u").string();
    Track a;
    a.path = (dir / "music" / "album" / "01 - Sailing.wav").string();
    a.title = "Sailing";
    a.durationSec = 0.4;
    PlayQueue queue;
    queue.setTracks({a}, 0);
    CHECK(queue.saveM3u(m3u));
    PlayQueue loaded;
    std::vector<std::string> paths;
    CHECK(loaded.loadM3u(m3u, &paths));
    CHECK(paths.size() == 1);
    CHECK(paths[0] == a.path);
}

// ------------------------------------------------------------------ engine --
void testEngine(const fs::path& dir) {
    section("audio engine");
    const std::string first = (dir / "engine_a.wav").string();
    const std::string second = (dir / "engine_b.wav").string();
    CHECK(writeSineWav(first, 1.0, 440.0, 44100, 2));
    CHECK(writeSineWav(second, 1.0, 660.0, 48000, 1));

    AudioEngine engine;
    std::string error;
    CHECK_MSG(engine.start(makeNullSink(8.0), &error), "engine starts on the null sink");
    CHECK(engine.running());

    std::atomic<int> stateChanges{0};
    std::atomic<bool> finished{false};
    std::atomic<bool> advanced{false};
    std::string advancedUri;
    std::atomic<double> lastPosition{0.0};
    engine.onStateChanged([&](PlaybackState) { ++stateChanges; });
    engine.onPositionChanged([&](double position, double) { lastPosition.store(position); });
    engine.onFinished([&] { finished.store(true); });
    engine.onTrackAdvanced([&](const std::string& uri) {
        advancedUri = uri;
        advanced.store(true);
    });

    CHECK_MSG(engine.load(first, true, &error), "loads a wav file");
    CHECK(waitFor([&] { return engine.state() == PlaybackState::Playing; }, 1000));
    CHECK_NEAR(engine.duration(), 1.0, 0.05);
    CHECK(waitFor([&] { return lastPosition.load() > 0.1; }, 2000));

    engine.setVolume(0.5f);
    CHECK_NEAR(engine.volume(), 0.5, 1e-6);
    engine.setVolume(5.0f);
    CHECK_MSG(engine.volume() <= 1.5f, "volume is clamped to the +150% boost ceiling");
    engine.setVolume(-2.0f);
    CHECK_MSG(engine.volume() == 0.0f, "negative volume is clamped to silence");
    engine.setVolume(1.0f);
    engine.setMuted(true);
    CHECK(engine.muted());
    engine.setMuted(false);
    engine.pause();
    CHECK(waitFor([&] { return engine.state() == PlaybackState::Paused; }, 500));
    engine.togglePlayPause();
    CHECK(waitFor([&] { return engine.state() == PlaybackState::Playing; }, 500));
    CHECK(engine.seek(0.2));
    CHECK(engine.seekRelative(0.1));
    CHECK(stateChanges.load() > 0);

    // Gapless: the engine asks for the next uri and switches without a gap.
    engine.setGapless(true);
    engine.setCrossfadeSeconds(0.0);
    std::atomic<bool> served{false};
    engine.setNextUriProvider([&]() -> std::string {
        if (served.exchange(true)) return std::string();
        return second;
    });
    CHECK_MSG(waitFor([&] { return advanced.load(); }, 6000), "gapless advance fires");
    CHECK(advancedUri == second);
    CHECK_MSG(waitFor([&] { return finished.load(); }, 8000), "finished fires at the very end");

    const EngineMetrics metrics = engine.metrics();
    CHECK(metrics.framesRendered > 48000);
    CHECK(metrics.decodedFrames > 0);
    CHECK_MSG(metrics.underruns == 0, "no underruns while rendering");
    std::cout << "    \033[2mrendered " << metrics.framesRendered << " frames, DSP "
              << str::formatDouble(metrics.renderCpuMsPerSecond, 3) << " ms/s, underruns "
              << metrics.underruns << "\033[0m\n";

    // Errors are reported, not fatal.
    std::atomic<bool> errored{false};
    engine.onError([&](const std::string&) { errored.store(true); });
    CHECK(!engine.load((dir / "does_not_exist.wav").string(), true, &error));
    CHECK(!error.empty());

    engine.stop();
    CHECK(engine.state() == PlaybackState::Stopped);
    engine.shutdown();
    CHECK(!engine.running());
}

void testCrossfade(const fs::path& dir) {
    section("crossfade");
    const std::string first = (dir / "cf_a.wav").string();
    const std::string second = (dir / "cf_b.wav").string();
    CHECK(writeSineWav(first, 1.0, 300.0, 44100, 2));
    CHECK(writeSineWav(second, 1.0, 500.0, 44100, 2));

    AudioEngine engine;
    std::string error;
    CHECK(engine.start(makeNullSink(6.0), &error));
    engine.setCrossfadeSeconds(0.4);
    CHECK_NEAR(engine.crossfadeSeconds(), 0.4, 1e-9);
    std::atomic<bool> advanced{false};
    engine.onTrackAdvanced([&](const std::string&) { advanced.store(true); });
    bool served = false;
    engine.setNextUriProvider([&]() -> std::string {
        if (served) return std::string();
        served = true;
        return second;
    });
    CHECK(engine.load(first, true, &error));
    CHECK_MSG(waitFor([&] { return advanced.load(); }, 6000), "crossfade hands over to the next track");
    CHECK(engine.metrics().underruns == 0);
    engine.shutdown();
}

// ---------------------------------------------------------------- analysis --
void testAnalysis(const fs::path& dir) {
    section("analysis");
    const std::string path = (dir / "halfsilent.wav").string();
    CHECK(writeSineWav(path, 1.0, 440.0, 44100, 1, 0.5));  // first half silent
    Waveform waveform;
    std::string error;
    CHECK(computeWaveform(path, 32, &waveform, DecoderOptions(), &error));
    CHECK(waveform.valid());
    CHECK(waveform.peaks.size() == 32);
    CHECK(waveform.rms.size() == 32);
    CHECK_NEAR(waveform.durationSec, 1.0, 0.1);
    CHECK_MSG(waveform.peaks[2] < 0.05f, "silence stays silent");
    CHECK_MSG(waveform.peaks[24] > 0.4f, "tone produces peaks");

    const std::string cache = (dir / "wave.json").string();
    CHECK(saveWaveform(cache, waveform));
    Waveform loaded;
    CHECK(loadWaveform(cache, &loaded));
    CHECK(loaded.peaks.size() == waveform.peaks.size());
    CHECK_NEAR(loaded.durationSec, waveform.durationSec, 1e-6);

    // Amber artwork -> warm dominant colour, dark background.
    const int width = 8, height = 8;
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width * height * 3));
    for (int i = 0; i < width * height; ++i) {
        pixels[i * 3 + 0] = 200;
        pixels[i * 3 + 1] = 120;
        pixels[i * 3 + 2] = 40;
    }
    const Palette palette = paletteFromPixels(pixels.data(), width, height, 3);
    CHECK(palette.dominant.r > palette.dominant.b);
    CHECK(palette.dominant.hex().size() == 7 && palette.dominant.hex()[0] == '#');
    CHECK(palette.accent.luminance() >= palette.muted.luminance());
    CHECK_MSG(std::fabs(palette.text.luminance() - palette.dominant.luminance()) > 0.25,
              "text colour contrasts with the artwork");
}

// -------------------------------------------------------------- downloader --
void testDownloader() {
    section("downloader");
    CHECK(Downloader::isYouTubeUrl("https://www.youtube.com/watch?v=abc"));
    CHECK(Downloader::isYouTubeUrl("https://youtu.be/abc"));
    CHECK(Downloader::isYouTubeUrl("https://music.youtube.com/watch?v=abc"));
    CHECK(!Downloader::isYouTubeUrl("https://example.com/song.mp3"));
    CHECK(Downloader::looksLikeDirectAudio("https://example.com/song.mp3"));
    CHECK(Downloader::looksLikeDirectAudio("https://cdn.site/track.flac?token=1"));
    CHECK(!Downloader::looksLikeDirectAudio("https://www.youtube.com/watch?v=abc"));

    Downloader downloader("/tmp", "yt-dlp", "ffmpeg");
    CHECK(downloader.enqueue("").empty());
    CHECK(downloader.jobs().empty());
}

// ----------------------------------------------------------------- process --
void testProcess() {
    section("process");
#ifdef _WIN32
    CHECK(Process::exists("cmd"));
#else
    CHECK(Process::exists("sh"));
#endif
    CHECK(!Process::exists("definitely-not-a-real-tool-xyz"));
    const Process::Result result = Process::run({"echo", "aurora"});
    CHECK(result.ok());
    CHECK(result.output.find("aurora") != std::string::npos);
    CHECK(!Process::commandLine({"a b", "c"}).empty());
}

// -------------------------------------------------------------- controller --
void testController(const fs::path& root) {
    section("controller (end to end)");
    Controller controller;
    std::string error;
    CHECK_MSG(controller.initialize(makeNullSink(8.0), &error), "controller initialises");
    if (!controller.initialized()) return;

    std::atomic<int> trackChanges{0};
    Controller::Callbacks callbacks;
    callbacks.trackChanged = [&](const Track&) { ++trackChanges; };
    controller.setCallbacks(callbacks);

    std::string message;
    const int added = controller.add((root / "music").string(), false, &message);
    CHECK_MSG(added >= 3, "folder import adds tracks");
    CHECK(controller.library().size() >= 3);

    std::vector<Track> tracks = controller.library().tracks();
    CHECK(controller.playTracks(tracks, 0, &error));
    CHECK(waitFor([&] { return controller.snapshot().state == PlaybackState::Playing; }, 1500));
    CHECK(!controller.currentTrack().path.empty());
    CHECK(trackChanges.load() > 0);
    CHECK(!controller.statusLine().empty());

    CHECK(controller.next());
    CHECK(waitFor([&] { return !controller.currentTrack().path.empty(); }, 1000));
    controller.setVolume(0.6f);
    CHECK_NEAR(controller.volume(), 0.6, 1e-6);
    controller.toggleMute();
    controller.toggleMute();
    controller.setSpeed(1.25);
    controller.cycleRepeat();
    CHECK(controller.repeat() == RepeatMode::All);
    controller.toggleShuffle();
    CHECK(controller.shuffle());
    controller.setCrossfade(1.0);
    controller.setGapless(true);
    controller.setEqualizerEnabled(true);
    CHECK(controller.applyEqualizerPreset("vocal"));
    controller.setEqualizerBand(0, 4.0f);
    controller.setPreampDb(-2.0f);
    controller.setLanguage("ru");
    CHECK(controller.config().settings().language == "ru");
    controller.setLanguage("en");
    CHECK(controller.toggleFavoriteCurrent());

    Waveform waveform;
    CHECK_MSG(controller.waveformForCurrent(48, &waveform) && waveform.valid(),
              "waveform for the current track (cached)");

    CHECK(controller.seek(0.1));
    CHECK(controller.previous());
    controller.pause();
    CHECK(waitFor([&] { return controller.snapshot().state == PlaybackState::Paused; }, 1000));
    controller.togglePlayPause();

    // Streams are added without touching the network.
    CHECK(controller.add("https://example.com/radio.mp3", false, &message) == 1);

    controller.persist();
    controller.shutdown();
    CHECK(!controller.initialized());

    Config config;
    CHECK(config.load());
    CHECK(config.settings().shuffle);
    CHECK(config.settings().repeat == RepeatMode::All);
}

} // namespace

int main() {
    Log::setLevel(LogLevel::Error);
    Log::setConsole(false);

    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root =
        fs::temp_directory_path() / ("aurora-tests-" + std::to_string(stamp % 1000000));
    fs::remove_all(root);
    fs::create_directories(root);
    setEnvVar("AURORA_CONFIG_DIR", (root / "config").string());
    setEnvVar("AURORA_DATA_DIR", (root / "data").string());
    setEnvVar("AURORA_CACHE_DIR", (root / "cache").string());
    setEnvVar("AURORA_LANG", "en");
    I18n::instance().setLanguage("en");

    std::cout << "\033[1mAurora Player core tests\033[0m  (" << root.string() << ")\n";

    const auto start = std::chrono::steady_clock::now();
    testStrings();
    testJson();
    testRingBuffer();
    testEqualizer();
    testDecoder(root);
    testTags(root);
    testLyrics(root);
    testI18n();
    testConfig(root);
    testLibrary(root);
    testQueue();
    testPlaylists(root);
    testM3u(root);
    testEngine(root);
    testCrossfade(root);
    testAnalysis(root);
    testDownloader();
    testProcess();
    testController(root);
    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

    std::cout << "\n\033[1m" << (gChecks - gFailures) << "/" << gChecks << " checks passed\033[0m in "
              << str::formatDouble(elapsed, 2) << " s\n";
    if (gFailures > 0) std::cout << "\033[31m" << gFailures << " failed\033[0m\n";

    fs::remove_all(root);
    return gFailures == 0 ? 0 : 1;
}
