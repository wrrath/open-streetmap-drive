#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
BUILD_TYPE="${BUILD_TYPE:-Debug}"

cd "$SCRIPT_DIR"

if ! command -v cmake >/dev/null 2>&1; then
  echo "Error: cmake is not installed." >&2
  exit 1
fi

if ! command -v glslc >/dev/null 2>&1 && ! command -v glslangValidator >/dev/null 2>&1; then
  echo "Warning: no GLSL shader compiler found." >&2
  echo "Install one with: sudo apt install glslang-tools" >&2
  echo "The game will run, but road mesh rendering may be unavailable." >&2
fi

cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build "$BUILD_DIR" -j"$(nproc)"

exec "$BUILD_DIR/osm_drive" "$@"
