// Aurora Player - GUI entry point.
//
// Wires the Qt Quick shell to the Qt-free C++17 core: one bridge object, four
// list models, one cover-art provider and a JSON-backed RU/EN translator.
//
// Every startup step is written to a log file and any fatal problem is shown
// in a native dialog, because this is a windowed application: without that a
// failure would simply close the process with no visible reason at all.
#include "CoverImageProvider.hpp"
#include "Models.hpp"
#include "PlayerBridge.hpp"
#include "QmlTranslator.hpp"

#include "aurora/Config.hpp"
#include "aurora/Log.hpp"
#include "aurora/Version.hpp"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QSurfaceFormat>
#include <QTextStream>
#include <QTimer>

#ifdef _WIN32
#  include <windows.h>
#endif

#include <cstdio>

namespace {

QString startupLogPath() {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) dir = QDir::tempPath();
    QDir().mkpath(dir);
    return dir + QStringLiteral("/startup.log");
}

void appendStartupLine(const QString& line) {
    static bool truncated = false;
    QFile file(startupLogPath());
    const QIODevice::OpenMode mode =
            truncated ? (QIODevice::Append | QIODevice::Text)
                      : (QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
    truncated = true;
    if (file.open(mode)) {
        QTextStream stream(&file);
        stream << QDateTime::currentDateTime().toString(Qt::ISODate) << QStringLiteral("  ")
               << line << Qt::endl;
    }
    std::fprintf(stderr, "%s\\n", line.toUtf8().constData());
}

void messageHandler(QtMsgType type, const QMessageLogContext&, const QString& text) {
    const char* label = "info";
    switch (type) {
    case QtDebugMsg: label = "debug"; break;
    case QtInfoMsg: label = "info"; break;
    case QtWarningMsg: label = "warning"; break;
    case QtCriticalMsg: label = "critical"; break;
    case QtFatalMsg: label = "fatal"; break;
    }
    appendStartupLine(QStringLiteral("[%1] %2").arg(QLatin1String(label), text));
}

void reportFatal(const QString& reason) {
    appendStartupLine(QStringLiteral("startup failed: ") + reason);
    if (qEnvironmentVariableIsSet("AURORA_NO_DIALOGS")) return;
#ifdef _WIN32
    const QString text = reason + QStringLiteral("\\n\\nA detailed log was written to:\\n")
            + QDir::toNativeSeparators(startupLogPath());
    MessageBoxW(nullptr, reinterpret_cast<const wchar_t*>(text.utf16()),
                L"Aurora Player", MB_OK | MB_ICONERROR);
#endif
}

} // namespace

int main(int argc, char* argv[]) {
    qInstallMessageHandler(messageHandler);
    appendStartupLine(QStringLiteral("Aurora Player %1 is starting")
                              .arg(QLatin1String(AURORA_VERSION_STRING)));

    // Software fallback keeps the blurred glass UI working on machines without
    // a usable GPU driver (remote desktops, old laptops, VMs).
    if (qEnvironmentVariableIsSet("AURORA_SOFTWARE_RENDER")) {
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
        appendStartupLine(QStringLiteral("using the software renderer"));
    }

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral(AURORA_APP_NAME));
    QGuiApplication::setOrganizationName(QStringLiteral(AURORA_ORG_NAME));
    QGuiApplication::setApplicationVersion(QStringLiteral(AURORA_VERSION_STRING));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/packaging/icons/aurora.svg")));
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    appendStartupLine(QStringLiteral("the application object was created"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral(
            "Aurora Player - offline and online music player (RU/EN)"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption selfTest(
            QStringLiteral("selftest"),
            QStringLiteral("Load the interface, confirm that it works and exit."));
    parser.addOption(selfTest);
    parser.addPositionalArgument(QStringLiteral("files"),
                                 QStringLiteral("Audio files, folders or links to open"));
    parser.process(app);

    aurora::Log::instance().setFile(aurora::Config::logPath());
    AURORA_LOG_INFO(std::string("Aurora Player ") + AURORA_VERSION_STRING + " starting");

    // ---- core + models ----------------------------------------------------
    aurora::LibraryModel libraryModel;
    aurora::AlbumModel albumModel;
    aurora::QueueModel queueModel;
    aurora::DownloadsModel downloadsModel;
    aurora::PlayerBridge bridge;

    QString initError;
    if (!bridge.initialize(&libraryModel, &albumModel, &queueModel, &downloadsModel, &initError)) {
        reportFatal(QStringLiteral("The player engine could not start: ") + initError);
        return 2;
    }
    appendStartupLine(QStringLiteral("the player engine is ready"));

    // ---- QML engine -------------------------------------------------------
    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("covers"), new aurora::CoverImageProvider);

    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &app,
                     [](const QList<QQmlError>& warnings) {
                         for (const QQmlError& warning : warnings)
                             appendStartupLine(QStringLiteral("interface: ") + warning.toString());
                     });

    QQmlContext* context = engine.rootContext();
    context->setContextProperty(QStringLiteral("player"), &bridge);
    context->setContextProperty(QStringLiteral("libraryModel"), &libraryModel);
    context->setContextProperty(QStringLiteral("albumModel"), &albumModel);
    context->setContextProperty(QStringLiteral("queueModel"), &queueModel);
    context->setContextProperty(QStringLiteral("downloadsModel"), &downloadsModel);
    context->setContextProperty(QStringLiteral("appVersion"),
                                QStringLiteral(AURORA_VERSION_STRING));

    // ---- runtime translations --------------------------------------------
    aurora::QmlTranslator russian;
    const QString dictionary = aurora::QmlTranslator::findDictionary(QStringLiteral("ru"));
    const bool hasRussian = !dictionary.isEmpty() && russian.loadDictionary(dictionary);
    bool russianInstalled = false;

    auto applyLanguage = [&](const QString& code) {
        const bool wantRussian = code == QStringLiteral("ru") && hasRussian;
        if (wantRussian == russianInstalled) return;
        if (wantRussian) QCoreApplication::installTranslator(&russian);
        else QCoreApplication::removeTranslator(&russian);
        russianInstalled = wantRussian;
        engine.retranslate();
    };
    applyLanguage(bridge.language());
    QObject::connect(&bridge, &aurora::PlayerBridge::languageChanged, &app,
                     [&] { applyLanguage(bridge.language()); });

    // ---- window -----------------------------------------------------------
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     []() {
                         reportFatal(QStringLiteral("The main window could not be created."));
                         QCoreApplication::exit(-1);
                     }, Qt::QueuedConnection);
    appendStartupLine(QStringLiteral("loading the interface"));
    engine.loadFromModule("Aurora", "Main");
    if (engine.rootObjects().isEmpty()) {
        reportFatal(QStringLiteral("The user interface could not be loaded."));
        return 3;
    }
    appendStartupLine(QStringLiteral("the interface is loaded and the window is open"));

    // Files passed on the command line (also used by "Open with" integrations).
    const QStringList positional = parser.positionalArguments();
    for (const QString& argument : positional) bridge.addPath(argument);

    if (parser.isSet(selfTest)) {
        appendStartupLine(QStringLiteral("self test: the application started correctly"));
        QTimer::singleShot(1500, &app, []() { QCoreApplication::exit(0); });
    }

    const int code = app.exec();
    appendStartupLine(QStringLiteral("Aurora Player is exiting with code %1").arg(code));
    AURORA_LOG_INFO("Aurora Player exiting");
    return code;
}
