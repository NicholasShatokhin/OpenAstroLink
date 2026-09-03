#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
preset="${1:-my-macos-arm64-observatory}"

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "ERROR: build_macos.sh must run on macOS (Darwin)." >&2
  exit 2
fi
if ! command -v cmake >/dev/null 2>&1; then
  echo "ERROR: cmake not found." >&2
  exit 3
fi
if ! command -v c++ >/dev/null 2>&1; then
  echo "ERROR: Apple Clang/C++ compiler not found. Install Xcode Command Line Tools." >&2
  exit 4
fi

echo "Host:  $(uname -s) $(uname -m)"
echo "CMake: $(cmake --version | head -n1)"
echo "C++:   $(c++ --version | head -n1)"
cmake --preset "$preset"
cmake --build --preset "$preset" -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
