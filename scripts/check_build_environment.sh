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
command -v ninja >/dev/null && echo "Ninja: $(ninja --version)" || echo "Ninja: NOT FOUND"
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
