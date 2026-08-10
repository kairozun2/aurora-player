# Aurora Player - Qt Quick interface.
qt_standard_project_setup(REQUIRES 6.5)
set(CMAKE_AUTOMOC ON)

# The Windows icon resource: without it Explorer shows a blank placeholder
# for the executable and for every shortcut that points at it.
set(AURORA_PLATFORM_RESOURCES)
if(WIN32)
    list(APPEND AURORA_PLATFORM_RESOURCES ${CMAKE_SOURCE_DIR}/packaging/icons/aurora.rc)
endif()

qt_add_executable(aurora-player WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/PlayerBridge.cpp
    src/PlayerBridge.hpp
    src/Models.cpp
    src/Models.hpp
    src/CoverImageProvider.hpp
    src/QmlTranslator.hpp
    src/QtAudioSink.hpp
    ${AURORA_PLATFORM_RESOURCES})

qt_add_qml_module(aurora-player
    URI Aurora
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/NowPlayingView.qml
        qml/LibraryView.qml
        qml/SettingsView.qml
        qml/EqualizerPanel.qml
        qml/QueuePanel.qml
        qml/LyricsPanel.qml
        qml/AddMusicDialog.qml
        qml/PlayerBar.qml
        qml/CoverFlow.qml
        qml/NowPlayingVinyl.qml
        qml/WaveformSeek.qml
        qml/VolumeControl.qml
        qml/TrackRow.qml
        qml/GlassCard.qml
        qml/IconButton.qml
        qml/IconGlyph.qml
        qml/AuroraButton.qml
        qml/AuroraSwitch.qml
        qml/SegmentedControl.qml
        qml/SettingsGroup.qml
        qml/SettingsRow.qml)

# Theme.qml uses "pragma Singleton", so it has to be flagged for the QML module.
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

# Dictionaries and the window icon are embedded: one self-contained binary.
qt_add_resources(aurora-player "aurora_assets"
    PREFIX "/"
    BASE ${CMAKE_SOURCE_DIR}
    FILES
        ${CMAKE_SOURCE_DIR}/i18n/ru.json
        ${CMAKE_SOURCE_DIR}/i18n/en.json
        ${CMAKE_SOURCE_DIR}/packaging/icons/aurora.svg)

target_include_directories(aurora-player PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(aurora-player PRIVATE
    Aurora::Core
    aurora_flags
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Multimedia)

target_compile_definitions(aurora-player PRIVATE AURORA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

set_target_properties(aurora-player PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "Aurora Player"
    MACOSX_BUNDLE_GUI_IDENTIFIER "audio.aurora.player"
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/packaging/Info.plist.in)

include(GNUInstallDirs)
install(TARGETS aurora-player
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

# windeployqt / macdeployqt style bundling of the Qt runtime.
qt_generate_deploy_qml_app_script(
    TARGET aurora-player
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR)
install(SCRIPT ${deploy_script})
