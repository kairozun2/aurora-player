# Aurora Player — core library (pure C++17, no external dependencies at compile time).
add_library(aurora_core STATIC
    src/Analysis.cpp
    src/AudioEngine.cpp
    src/Config.cpp
    src/Controller.cpp
    src/Decoder.cpp
    src/Downloader.cpp
    src/Equalizer.cpp
    src/FfmpegDecoder.cpp
    src/I18n.cpp
    src/Json.cpp
    src/Log.cpp
    src/Lyrics.cpp
    src/MediaLibrary.cpp
    src/Playlist.cpp
    src/Process.cpp
    src/Sinks.cpp
    src/Strings.cpp
    src/TagReader.cpp
    src/WavDecoder.cpp)

# Canonical alias used by all other targets: Aurora::Core
add_library(Aurora::Core ALIAS aurora_core)

target_include_directories(aurora_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

target_link_libraries(aurora_core PUBLIC aurora_flags)

# Threads (std::thread / std::mutex) require explicit linking on POSIX.
find_package(Threads REQUIRED)
target_link_libraries(aurora_core PUBLIC Threads::Threads)

include(GNUInstallDirs)
install(TARGETS aurora_core
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
