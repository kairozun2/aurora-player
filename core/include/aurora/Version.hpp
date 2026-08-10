// Aurora Player - version metadata
#pragma once

#define AURORA_VERSION_MAJOR 1
#define AURORA_VERSION_MINOR 0
#define AURORA_VERSION_PATCH 0
#define AURORA_VERSION_STRING "1.0.0"
#define AURORA_APP_NAME "Aurora Player"
#define AURORA_ORG_NAME "AuroraAudio"

namespace aurora {
inline const char* versionString() { return AURORA_VERSION_STRING; }
inline const char* appName() { return AURORA_APP_NAME; }
} // namespace aurora
