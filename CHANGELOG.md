# Changelog

All notable changes to Aurora Player are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)
and the project uses [Semantic Versioning](https://semver.org/).

## [1.0.0] - 2026-08-10

### Added
- Lock-free audio engine: ring buffer, decoder thread, gapless playback,
  crossfade, speed control (0.5x-2x), 10-band equalizer with 11 presets,
  per-channel level metering and engine telemetry (underruns, DSP load).
- Universal decoding: built-in WAV reader plus an ffmpeg pipe decoder for MP3,
  FLAC, AAC/M4A, OGG/Vorbis, Opus, WMA, ALAC and HTTP streams.
- Media library with incremental scanning, JSON index, search with Cyrillic
  transliteration, albums, artists, genres, favourites, play counts.
- Imports: local files, whole folders, .m3u playlists, direct audio links,
  and YouTube (and 1000+ other sites) through yt-dlp with progress reporting.
- Tag reading for ID3v1/ID3v2, FLAC/Vorbis comments and MP4 atoms, embedded
  cover art extraction and sidecar cover/lyrics discovery.
- Synced .lrc lyrics with click-to-seek, plain lyrics fallback.
- Waveform analysis with on-disk cache and cover-art palette extraction.
- Full Russian and English interface, switchable at runtime.
- Qt Quick interface: cover-flow carousel, vinyl now-playing card, glass
  player bar, library, queue, lyrics, equalizer and settings screens.
- Command line player `aurora-cli` with 17 commands including `doctor`,
  `bench`, `waveform` and offline WAV rendering.
- Cross-platform build via CMake, CI for Linux/Windows/macOS, MIT licence.
