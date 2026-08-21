#!/usr/bin/env bash
set -euo pipefail
preset="${1:-linux-observatory-release}"
cmake --preset "$preset"
cmake --build --preset "$preset" -j"$(nproc 2>/dev/null || echo 4)"
