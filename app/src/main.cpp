// Aurora Player - GUI entry point.
//
// Wires the Qt Quick shell to the Qt-free C++17 core: one bridge object, four
// list models, one cover-art provider and a JSON-backed RU/EN translator.
#include "CoverImageProvider.hpp"
#include "Models.hpp"
#include "PlayerBridge.hpp"
#include "QmlTranslator.hpp"

#include "aurora/Config.hpp"
#include "aurora/Log.hpp"
#include "aurora/Version.hpp"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSurfaceFormat>

#include <cstdio>

int main(int argc, char* argv[]) {
    // Software fallback keeps the blurred glass UI working on machines without
    // a usable GPU driver (remote desktops, old laptops, VMs).
    if (qEnvironmentVariableIsSet("AURORA_SOFTWARE_RENDER")) {
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    }

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral(AURORA_APP_NAME));
    QGuiApplication::setOrganizationName(QStringLiteral(AURORA_ORG_NAME));
    QGuiApplication::setApplicationVersion(QStringLiteral(AURORA_VERSION_STRING));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/aurora.svg")));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral(
            "Aurora Player - offline and online music player (RU/EN)"));
    parser.addHelpOption();
    parser.addVersionOption();
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
        std::fprintf(stderr, "Aurora: %s\n", initError.toUtf8().constData());
        return 2;
    }

    // ---- QML engine -------------------------------------------------------
    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("covers"), new aurora::CoverImageProvider);

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
                     []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule("Aurora", "Main");
    if (engine.rootObjects().isEmpty()) {
        std::fprintf(stderr, "Aurora: failed to load the QML interface\n");
        return 3;
    }

    // Files passed on the command line (also used by "Open with" integrations).
    const QStringList positional = parser.positionalArguments();
    for (const QString& argument : positional) bridge.addPath(argument);

    const int code = app.exec();
    AURORA_LOG_INFO("Aurora Player exiting");
    return code;
}
