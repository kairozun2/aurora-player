#!/usr/bin/env bash
# Aurora Player - one-command build for Linux and macOS.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build/release}"

if ! command -v cmake >/dev/null 2>&1; then
  echo "cmake is required: https://cmake.org/download/" >&2
  exit 1
fi

echo "==> Configuring ($BUILD_DIR)"
cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release "$@"

echo "==> Building"
cmake --build "$BUILD_DIR" --parallel

echo "==> Testing"
ctest --test-dir "$BUILD_DIR" --output-on-failure || true

echo
echo "Done. Binaries:"
find "$BUILD_DIR/bin" -maxdepth 2 -type f -perm -u+x 2>/dev/null | sed 's/^/  /'
echo
echo "Run the GUI:  $BUILD_DIR/bin/aurora-player"
echo "Run the CLI:  $BUILD_DIR/bin/aurora-cli --help"
