#!/usr/bin/env bash
set -euo pipefail
cmake --preset default-release
cmake --build --preset default-release -j"$(nproc 2>/dev/null || echo 4)"
