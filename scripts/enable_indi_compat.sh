#!/usr/bin/env bash
set -euo pipefail
if [[ $EUID -ne 0 ]]; then
  echo "Run with sudo: sudo $0 [indi_driver_executable ...]" >&2
  exit 1
fi

export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y indi-bin
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

cat <<'MSG'
INDI runtime installed.

OAL treats INDI as a compatibility layer. Native QHY/Gemini/Sky-Watcher
remains the preferred path, while INDI can be present for other equipment.

Build choices:
  Native-only proof/minimal build:
    cmake --preset rpi4-native-release
    # equivalent core option: -DOAS_ENABLE_INDI=OFF

  Recommended observatory build with compatibility available:
    cmake --preset rpi4-observatory-release
    # equivalent core option: -DOAS_ENABLE_INDI=ON

Compiling INDI support does not require indiserver to be running. Start an
INDI service only when an INDI-only device is actually needed.
MSG

if [[ $# -gt 0 ]]; then
  exec "$ROOT_DIR/scripts/install_indi_service.sh" "$@"
fi

echo "Install any device-specific INDI packages you need, then run:"
echo "  sudo ./scripts/install_indi_service.sh indi_driver_one [indi_driver_two ...]"
echo "Examples available on this machine:"
find /usr/bin -maxdepth 1 -type f -name 'indi_*' -printf '  %f\n' 2>/dev/null | sort | head -80 || true
