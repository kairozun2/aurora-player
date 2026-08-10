cmake_minimum_required(VERSION 3.21)

project(AuroraPlayer
    VERSION 1.0.0
    DESCRIPTION "Aurora Player - offline and online music player in modern C++"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(AURORA_BUILD_GUI "Build the Qt Quick interface" ON)
option(AURORA_BUILD_CLI "Build the command line player" ON)
option(AURORA_BUILD_TESTS "Build unit tests" ON)
option(AURORA_LTO "Enable link time optimisation for release builds" ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ---------------------------------------------------------------------------
# Shared compile settings for every Aurora target.
# ---------------------------------------------------------------------------
add_library(aurora_flags INTERFACE)
target_compile_features(aurora_flags INTERFACE cxx_std_17)

if(MSVC)
    target_compile_options(aurora_flags INTERFACE /utf-8 /W4 /permissive- /EHsc
        $<$<CONFIG:Release>:/O2> $<$<CONFIG:Release>:/Gy>)
    target_compile_definitions(aurora_flags INTERFACE
        NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS UNICODE _UNICODE)
else()
    target_compile_options(aurora_flags INTERFACE
        -Wall -Wextra -Wpedantic -Wno-unused-parameter
        $<$<CONFIG:Release>:-O2> $<$<CONFIG:Release>:-fno-math-errno>)
endif()

if(AURORA_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT aurora_ipo_ok OUTPUT aurora_ipo_msg)
    if(aurora_ipo_ok)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    endif()
endif()

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------
add_subdirectory(core)

if(AURORA_BUILD_CLI)
    add_subdirectory(cli)
endif()

if(AURORA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

set(AURORA_GUI_ENABLED OFF)
if(AURORA_BUILD_GUI)
    find_package(Qt6 6.5 QUIET COMPONENTS Core Gui Quick QuickControls2 Multimedia)
    if(Qt6_FOUND)
        set(AURORA_GUI_ENABLED ON)
        add_subdirectory(app)
    else()
        message(WARNING
            "Qt 6.5+ (Quick, QuickControls2, Multimedia) was not found - the graphical "
            "interface will be skipped. The core library, the CLI player and the tests "
            "still build. Install Qt and re-run CMake, or configure with "
            "-DAURORA_BUILD_GUI=OFF to silence this warning.")
    endif()
endif()

# ---------------------------------------------------------------------------
# Installation extras
# ---------------------------------------------------------------------------
include(GNUInstallDirs)

install(DIRECTORY i18n/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/aurora-player/i18n
    FILES_MATCHING PATTERN "*.json")

if(UNIX AND NOT APPLE)
    install(FILES packaging/aurora-player.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES packaging/icons/aurora.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
        RENAME aurora-player.svg)
endif()

message(STATUS "")
message(STATUS "Aurora Player ${PROJECT_VERSION}")
message(STATUS "  build type ....... ${CMAKE_BUILD_TYPE}")
message(STATUS "  Qt Quick GUI ..... ${AURORA_GUI_ENABLED}")
message(STATUS "  CLI player ....... ${AURORA_BUILD_CLI}")
message(STATUS "  unit tests ....... ${AURORA_BUILD_TESTS}")
message(STATUS "")
