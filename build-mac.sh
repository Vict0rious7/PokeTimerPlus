#!/bin/bash
# Build PokeTimerPlus on a Mac in one command.
#   ./build-mac.sh          build, then open the app
#   ./build-mac.sh --test   build with the tests, run them, then open the app
#   ./build-mac.sh --clean  start again from an empty build folder (can be combined with --test)
# Needs Homebrew (https://brew.sh); installs what's missing (Qt, CMake, Ninja).
set -euo pipefail
cd "$(dirname "$0")"

TEST=""
for arg in "$@"; do
    case "$arg" in
        --test) TEST="-DTEST=ON" ;;
        --clean) rm -rf build ;;
        *) echo "Unknown option: $arg (use --test or --clean)"; exit 1 ;;
    esac
done

command -v brew >/dev/null || { echo "Homebrew is missing: install it from https://brew.sh, then run this again."; exit 1; }
brew list --versions qt >/dev/null || brew install qt
command -v cmake >/dev/null || brew install cmake
command -v ninja >/dev/null || brew install ninja
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release $TEST \
    -DCMAKE_PREFIX_PATH="$(brew --prefix qt)" -DCMAKE_OSX_ARCHITECTURES="$(uname -m)"
cmake --build build
if [ -n "$TEST" ]; then
    ctest --test-dir build --output-on-failure
fi
open "build/PokeTimerPlus.app"
