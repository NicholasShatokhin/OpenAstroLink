#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
preset="${1:-my-linux-observatory}"
"$root/scripts/check_build_environment.sh"
cmake --preset "$preset"
cmake --build --preset "$preset" -j"$(nproc 2>/dev/null || echo 4)"
