# Contributing to Aurora Player

Thanks for helping out! The project has two layers and they stay independent:

- `core/` - Qt-free C++17: engine, decoders, library, tags, downloads, i18n.
  Everything here must compile with just a C++17 compiler and be unit tested.
- `app/` - Qt 6 Quick interface. No audio logic lives here, only presentation
  and the `PlayerBridge` that exposes the core to QML.

## Getting started

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

No Qt installed? Configure with `-DAURORA_BUILD_GUI=OFF` and you still get the
library, the CLI and the tests.

## Rules of the road

1. `clang-format` with the bundled `.clang-format` before sending a patch.
2. Every core change needs a test in `tests/test_core.cpp`.
3. Never allocate, lock a mutex or log inside the audio render callback.
4. New user-visible strings go through `tr()` / `qsTr()` and get a Russian
   translation in `i18n/ru.json` (and `core/src/I18n.cpp` for core strings).
5. Keep pull requests focused; describe what you measured, not just what you
   changed (underruns, DSP load, scan time).
