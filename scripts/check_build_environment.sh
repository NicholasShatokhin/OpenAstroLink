#!/usr/bin/env bash
set -euo pipefail

min_major=3
min_minor=20

ver="$(cmake --version 2>/dev/null | awk 'NR==1{print $3}')"
if [[ -z "${ver:-}" ]]; then
  echo "ERROR: cmake was not found in PATH." >&2
  exit 2
fi
major="${ver%%.*}"
rest="${ver#*.}"
minor="${rest%%.*}"
if (( major < min_major || (major == min_major && minor < min_minor) )); then
  echo "ERROR: CMake >= ${min_major}.${min_minor} is required; found ${ver}." >&2
  echo "Install a newer CMake, then retry. The repository intentionally uses preset schema v2 for broad compatibility." >&2
  exit 3
fi

echo "CMake: ${ver}"
echo "Host:  $(uname -s 2>/dev/null || echo unknown)"
echo "Arch:  $(uname -m 2>/dev/null || echo unknown)"

if [[ -r /etc/os-release ]]; then
  # shellcheck disable=SC1091
  . /etc/os-release
  echo "Distro: ${PRETTY_NAME:-${ID:-unknown} ${VERSION_ID:-}}"
fi

qt_ver=""
if command -v qtpaths6 >/dev/null 2>&1; then
  qt_ver="$(qtpaths6 --qt-version 2>/dev/null || true)"
elif command -v qmake6 >/dev/null 2>&1; then
  qt_ver="$(qmake6 -query QT_VERSION 2>/dev/null || true)"
elif command -v pkg-config >/dev/null 2>&1; then
  qt_ver="$(pkg-config --modversion Qt6Core 2>/dev/null || true)"
fi
if [[ -n "$qt_ver" ]]; then
  echo "Qt6:   ${qt_ver}"
  if [[ "$(printf '%s\n' '6.4.0' "$qt_ver" | sort -V | head -n1)" != "6.4.0" ]]; then
    echo "ERROR: OpenAstroLink requires Qt >= 6.4; detected ${qt_ver}." >&2
    echo "       Qt6::HttpServer is part of the required node REST stack." >&2
    echo "       Ubuntu 22.04/Jammy system Qt 6.2.4 is not a supported build baseline." >&2
    echo "       Use Ubuntu 24.04+, Debian 12+/Raspberry Pi OS Bookworm+, or a custom Qt >= 6.4." >&2
    exit 5
  fi
else
  echo "Qt6:   not detected in PATH/pkg-config (CMake may still find a custom prefix)."
fi
command -v ninja >/dev/null && echo "Ninja: $(ninja --version)" || echo "Ninja: not installed (Linux presets use Unix Makefiles; Windows presets still use Ninja)"
command -v make >/dev/null && echo "Make:  $(make --version | head -n1)" || echo "Make: NOT FOUND"
command -v g++ >/dev/null && echo "C++:   $(g++ --version | head -n1)" || echo "C++: system g++ not found"
command -v pkg-config >/dev/null && echo "pkg-config: $(pkg-config --version)" || echo "pkg-config: NOT FOUND"

echo
if [[ -f CMakeUserPresets.json ]]; then
  python3 - <<'PY'
import json
from pathlib import Path
p=Path('CMakeUserPresets.json')
try:
    d=json.loads(p.read_text())
except Exception as e:
    raise SystemExit(f'ERROR: {p}: {e}')
v=d.get('version')
print(f'CMakeUserPresets.json schema version: {v}')
if not isinstance(v, int) or v > 2:
    print('WARNING: this user preset uses a newer schema than the repository baseline. On older Linux CMake versions, change it to version 2 and remove unsupported fields.')
PY
else
  echo "CMakeUserPresets.json: not present (copy CMakeUserPresets.example.json and edit SDK paths)."
fi

echo
cmake --list-presets
