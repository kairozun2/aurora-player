#include "aurora/I18n.hpp"

#include "aurora/Json.hpp"
#include "aurora/Log.hpp"
#include "aurora/Strings.hpp"

#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

namespace aurora {
namespace {

struct Entry {
    const char* key;
    const char* en;
    const char* ru;
};

// Built-in catalogue. Keys are stable identifiers used by the UI and the CLI.
const Entry kStrings[] = {
    {"app.name", "Aurora Player", "Aurora Player"},
    {"app.tagline", "Your music, everywhere", "Твоя музыка — всегда рядом"},

    {"nav.home", "Home", "Главная"},
    {"nav.library", "Library", "Медиатека"},
    {"nav.albums", "Albums", "Альбомы"},
    {"nav.artists", "Artists", "Исполнители"},
    {"nav.playlists", "Playlists", "Плейлисты"},
    {"nav.favorites", "Favorites", "Избранное"},
    {"nav.recent", "Recently added", "Недавно добавленные"},
    {"nav.downloads", "Downloads", "Загрузки"},
    {"nav.settings", "Settings", "Настройки"},

    {"player.play", "Play", "Играть"},
    {"player.pause", "Pause", "Пауза"},
    {"player.stop", "Stop", "Стоп"},
    {"player.next", "Next track", "Следующий трек"},
    {"player.previous", "Previous track", "Предыдущий трек"},
    {"player.shuffle", "Shuffle", "Перемешать"},
    {"player.repeat", "Repeat", "Повтор"},
    {"player.repeat.off", "Repeat off", "Повтор выключен"},
    {"player.repeat.all", "Repeat all", "Повторять всё"},
    {"player.repeat.one", "Repeat one", "Повторять трек"},
    {"player.volume", "Volume", "Громкость"},
    {"player.mute", "Mute", "Без звука"},
    {"player.speed", "Speed", "Скорость"},
    {"player.lyrics", "Lyrics", "Текст песни"},
    {"player.queue", "Queue", "Очередь"},
    {"player.nowPlaying", "Now playing", "Сейчас играет"},
    {"player.listeningTo", "Listening to", "Сейчас слушаю"},
    {"player.refresh", "Refresh", "Обновить"},
    {"player.back", "Back", "Назад"},
    {"player.openSource", "Open source", "Открыть источник"},
    {"player.favorite", "Add to favorites", "В избранное"},
    {"player.unfavorite", "Remove from favorites", "Убрать из избранного"},
    {"player.visualizer", "Visualizer", "Визуализация"},
    {"player.crossfade", "Crossfade", "Плавный переход"},
    {"player.gapless", "Gapless playback", "Без пауз между треками"},

    {"library.title", "Library", "Медиатека"},
    {"library.empty", "Your library is empty", "Медиатека пуста"},
    {"library.emptyHint", "Add files, a folder or a link to get started",
     "Добавьте файлы, папку или ссылку, чтобы начать"},
    {"library.tracks", "Tracks", "Треки"},
    {"library.search", "Search", "Поиск"},
    {"library.searchPlaceholder", "Search tracks, artists, albums…",
     "Поиск треков, исполнителей, альбомов…"},
    {"library.scan", "Scan folders", "Сканировать папки"},
    {"library.scanning", "Scanning…", "Сканирование…"},
    {"library.scanDone", "Scan finished", "Сканирование завершено"},
    {"library.noResults", "Nothing found", "Ничего не найдено"},
    {"library.stats", "{0} tracks · {1} albums · {2} artists",
     "{0} треков · {1} альбомов · {2} исполнителей"},

    {"add.title", "Add music", "Добавить музыку"},
    {"add.file", "Audio files", "Аудиофайлы"},
    {"add.folder", "Folder", "Папка"},
    {"add.link", "Direct link", "Прямая ссылка"},
    {"add.youtube", "YouTube / video link", "YouTube / ссылка на видео"},
    {"add.stream", "Radio stream", "Радиопоток"},
    {"add.urlPlaceholder", "Paste a link: YouTube, mp3, radio stream…",
     "Вставьте ссылку: YouTube, mp3, радиопоток…"},
    {"add.download", "Download", "Скачать"},
    {"add.playNow", "Play now", "Играть сейчас"},
    {"add.hint", "Files stay on your computer and work offline",
     "Файлы остаются на компьютере и работают офлайн"},

    {"download.title", "Downloads", "Загрузки"},
    {"download.empty", "No downloads yet", "Загрузок пока нет"},
    {"download.queued", "Queued", "В очереди"},
    {"download.running", "Downloading", "Загружается"},
    {"download.completed", "Completed", "Готово"},
    {"download.failed", "Failed", "Ошибка"},
    {"download.cancelled", "Cancelled", "Отменено"},
    {"download.speed", "Speed", "Скорость"},
    {"download.eta", "Remaining", "Осталось"},
    {"download.needYtDlp", "Install yt-dlp to import from YouTube",
     "Установите yt-dlp, чтобы импортировать с YouTube"},
    {"download.needFfmpeg", "Install ffmpeg for full format support",
     "Установите ffmpeg для поддержки всех форматов"},

    {"eq.title", "Equalizer", "Эквалайзер"},
    {"eq.enable", "Enable equalizer", "Включить эквалайзер"},
    {"eq.preset", "Preset", "Пресет"},
    {"eq.preamp", "Preamp", "Предусиление"},
    {"eq.reset", "Reset", "Сбросить"},
    {"eq.preset.flat", "Flat", "Ровный"},
    {"eq.preset.bass", "Bass boost", "Усиление басов"},
    {"eq.preset.treble", "Treble boost", "Усиление высоких"},
    {"eq.preset.vocal", "Vocal", "Вокал"},
    {"eq.preset.rock", "Rock", "Рок"},
    {"eq.preset.pop", "Pop", "Поп"},
    {"eq.preset.jazz", "Jazz", "Джаз"},
    {"eq.preset.classical", "Classical", "Классика"},
    {"eq.preset.electronic", "Electronic", "Электроника"},
    {"eq.preset.podcast", "Podcast", "Подкаст"},
    {"eq.preset.night", "Night mode", "Ночной режим"},
    {"eq.preset.custom", "Custom", "Свой"},

    {"queue.title", "Play queue", "Очередь воспроизведения"},
    {"queue.empty", "Queue is empty", "Очередь пуста"},
    {"queue.upcoming", "Up next", "Далее"},
    {"queue.clear", "Clear queue", "Очистить очередь"},
    {"queue.playNext", "Play next", "Играть следующим"},
    {"queue.remove", "Remove", "Убрать"},
    {"queue.saveAsPlaylist", "Save as playlist", "Сохранить как плейлист"},

    {"lyrics.title", "Lyrics", "Текст песни"},
    {"lyrics.empty", "No lyrics for this track", "Для этого трека нет текста"},
    {"lyrics.hint", "Put a .lrc file next to the audio file",
     "Положите файл .lrc рядом с аудиофайлом"},

    {"settings.title", "Settings", "Настройки"},
    {"settings.language", "Language", "Язык"},
    {"settings.theme", "Theme", "Тема"},
    {"settings.theme.light", "Light", "Светлая"},
    {"settings.theme.dark", "Dark", "Тёмная"},
    {"settings.theme.auto", "System", "Как в системе"},
    {"settings.audio", "Audio", "Звук"},
    {"settings.sampleRate", "Sample rate", "Частота дискретизации"},
    {"settings.folders", "Music folders", "Папки с музыкой"},
    {"settings.downloadDir", "Download folder", "Папка загрузок"},
    {"settings.tools", "External tools", "Внешние утилиты"},
    {"settings.rememberPosition", "Resume where I left off", "Продолжать с того же места"},
    {"settings.scanOnStart", "Scan folders on start", "Сканировать папки при запуске"},
    {"settings.about", "About", "О программе"},
    {"settings.installed", "installed", "установлено"},
    {"settings.missing", "not found", "не найдено"},

    {"common.ok", "OK", "OK"},
    {"common.cancel", "Cancel", "Отмена"},
    {"common.close", "Close", "Закрыть"},
    {"common.save", "Save", "Сохранить"},
    {"common.add", "Add", "Добавить"},
    {"common.remove", "Remove", "Удалить"},
    {"common.error", "Error", "Ошибка"},
    {"common.loading", "Loading…", "Загрузка…"},
    {"common.unknownArtist", "Unknown artist", "Неизвестный исполнитель"},
    {"common.unknownAlbum", "Unknown album", "Неизвестный альбом"},
    {"common.tracks", "{0} tracks", "{0} треков"},
    {"common.of", "of", "из"},
    {"time.total", "Total time", "Общая длительность"},

    {"error.decode", "Cannot play this file", "Не удалось воспроизвести файл"},
    {"error.notFound", "File not found", "Файл не найден"},
    {"error.network", "Network error", "Ошибка сети"},
};

} // namespace

I18n::I18n() {
    std::map<std::string, std::string> en;
    std::map<std::string, std::string> ru;
    for (const Entry& entry : kStrings) {
        en[entry.key] = entry.en;
        ru[entry.key] = entry.ru;
    }
    catalogs_["en"] = en;
    catalogs_["ru"] = ru;
    language_ = detectSystemLanguage();
}

I18n& I18n::instance() {
    static I18n instance;
    return instance;
}

std::string I18n::detectSystemLanguage() {
    const char* variables[] = {"AURORA_LANG", "LC_ALL", "LC_MESSAGES", "LANG", "LANGUAGE"};
    for (const char* variable : variables) {
        const char* value = std::getenv(variable);
        if (!value || !*value) continue;
        const std::string lower = str::toLower(value);
        if (str::startsWith(lower, "ru")) return "ru";
        if (str::startsWith(lower, "en")) return "en";
    }
    return "en";
}

std::string I18n::languageName(const std::string& code) {
    if (code == "ru") return "Русский";
    if (code == "en") return "English";
    return code;
}

bool I18n::setLanguage(const std::string& code) {
    const std::string wanted = (code == "system" || code.empty()) ? detectSystemLanguage() : str::toLower(code);
    std::lock_guard<std::mutex> lock(mutex_);
    if (catalogs_.find(wanted) == catalogs_.end()) return false;
    language_ = wanted;
    return true;
}

const std::string& I18n::language() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return language_;
}

std::vector<std::string> I18n::availableLanguages() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> codes;
    for (const auto& entry : catalogs_) codes.push_back(entry.first);
    return codes;
}

const std::map<std::string, std::string>* I18n::catalog(const std::string& code) const {
    const auto found = catalogs_.find(code);
    return found == catalogs_.end() ? nullptr : &found->second;
}

std::string I18n::tr(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (const auto* active = catalog(language_)) {
        const auto found = active->find(key);
        if (found != active->end() && !found->second.empty()) return found->second;
    }
    if (const auto* fallback = catalog("en")) {
        const auto found = fallback->find(key);
        if (found != fallback->end()) return found->second;
    }
    return key;
}

std::string I18n::tr(const std::string& key, const std::vector<std::string>& args) const {
    std::string text = tr(key);
    for (std::size_t i = 0; i < args.size(); ++i) {
        text = str::replaceAll(text, "{" + std::to_string(i) + "}", args[i]);
    }
    return text;
}

void I18n::define(const std::string& language, const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    catalogs_[language][key] = value;
}

int I18n::loadDirectory(const std::string& dir) {
    std::error_code ec;
    if (!fs::is_directory(fs::path(dir), ec)) return 0;
    int loaded = 0;
    for (const fs::directory_entry& entry : fs::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        if (str::extension(entry.path().string()) != "json") continue;
        const std::string code = str::toLower(entry.path().stem().string());
        std::string error;
        const Json root = Json::parseFile(entry.path().string(), &error);
        if (!error.empty() || !root.isObject()) continue;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& item : root.items()) {
                catalogs_[code][item.first] = item.second.asString();
            }
        }
        ++loaded;
        logInfo("i18n", "loaded overrides for " + code);
    }
    return loaded;
}

std::string tr(const std::string& key) { return I18n::instance().tr(key); }
std::string tr(const std::string& key, const std::vector<std::string>& args) {
    return I18n::instance().tr(key, args);
}

} // namespace aurora
