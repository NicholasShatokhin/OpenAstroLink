#!/usr/bin/env bash
set -euo pipefail

if [[ $EUID -ne 0 ]]; then
  echo "Run with sudo: sudo $0" >&2
  exit 1
fi

export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y \
  build-essential cmake ninja-build pkg-config git curl ca-certificates \
  qt6-base-dev qt6-serialport-dev qt6-websockets-dev qt6-httpserver-dev \
  libopencv-dev \
  indi-bin libindi-dev

# Optional Qt Positioning is used when present, but is not required for a node
# with a configured observatory location.
if apt-cache show qt6-positioning-dev >/dev/null 2>&1; then
  apt-get install -y qt6-positioning-dev || true
fi

OAL_USER="${SUDO_USER:-pi}"
for group in dialout plugdev video; do
  if getent group "$group" >/dev/null 2>&1; then
    usermod -aG "$group" "$OAL_USER"
  fi
done

cat <<MSG
Base Raspberry Pi observatory dependencies are installed.

Still hardware/vendor specific and intentionally not auto-downloaded:
  1. QHYCCD ARM64/Raspberry-Pi SDK + its udev rules.
     This builds the native ABI-v2 oal.qhy driver; INDI is not required for QHY.
     Verify qhyccd.h and libqhyccd.so are installed, or set QHYCCD_ROOT at CMake configure time.
  2. The exact INDI drivers for your mount and Gemini focuser IF native OAL drivers are not yet available.
     INDI is a compatibility path, not the reference architecture.
  3. ASTAP/astap_cli ARM package and ONE compatible ASTAP star database.

After installing those pieces run:
  oal-hardware-probe          # after the project is built/installed
or, from the build tree:
  ./build/rpi4-observatory/oal-hardware-probe

User '$OAL_USER' was added to available dialout/plugdev/video groups.
Log out/reboot for new group membership to take effect.
MSG
