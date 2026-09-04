#!/usr/bin/env bash
set -euo pipefail

if [[ $EUID -ne 0 ]]; then
  echo "Run with sudo: sudo $0 [--with-indi]" >&2
  exit 1
fi
WITH_INDI=0
for a in "$@"; do
  case "$a" in
    --with-indi) WITH_INDI=1 ;;
    *) echo "Unknown option: $a" >&2; exit 2 ;;
  esac
done

export DEBIAN_FRONTEND=noninteractive

if [[ -r /etc/os-release ]]; then
  # shellcheck disable=SC1091
  . /etc/os-release
fi
case "${ID:-}" in
  ubuntu)
    if [[ "${VERSION_ID:-0}" != "24.04" && "${VERSION_ID:-0}" != "24.10" && "${VERSION_ID:-0}" != "25.04" && "${VERSION_ID:-0}" != "25.10" && "${VERSION_ID:-0}" != "26.04" ]]; then
      echo "ERROR: Ubuntu ${VERSION_ID:-unknown} system Qt is not a supported OAL baseline." >&2
      echo "       Use Ubuntu 24.04+ (Qt >= 6.4 with Qt HttpServer)." >&2
      exit 4
    fi
    ;;
  debian|raspbian)
    major="${VERSION_ID%%.*}"
    if [[ -n "${major:-}" ]] && (( major < 12 )); then
      echo "ERROR: Debian/Raspberry Pi OS ${VERSION_ID:-unknown} is too old for the OAL Qt >= 6.4 baseline." >&2
      echo "       Use Bookworm (12) or newer." >&2
      exit 4
    fi
    ;;
esac

apt-get update
required_packages=(
  build-essential cmake pkg-config git curl ca-certificates file
  qt6-base-dev qt6-serialport-dev qt6-websockets-dev qt6-httpserver-dev
  libopencv-dev libgphoto2-dev libjpeg-dev
)
for pkg in "${required_packages[@]}"; do
  if ! apt-cache show "$pkg" >/dev/null 2>&1; then
    echo "ERROR: required package '$pkg' is unavailable in the configured repositories." >&2
    echo "       OAL's system-Qt baseline is Ubuntu 24.04+ or Debian/Raspberry Pi OS 12+ (Bookworm)." >&2
    exit 5
  fi
done
apt-get install -y "${required_packages[@]}"

if [[ $WITH_INDI -eq 1 ]]; then
  apt-get install -y indi-bin
fi

if apt-cache show qt6-positioning-dev >/dev/null 2>&1; then
  apt-get install -y qt6-positioning-dev || true
fi

OAL_USER="${SUDO_USER:-pi}"
for group in dialout plugdev video; do
  if getent group "$group" >/dev/null 2>&1; then usermod -aG "$group" "$OAL_USER"; fi
done

cat <<MSG
Base Raspberry Pi observatory dependencies are installed.

Native reference path:
  QHY camera   -> oal.qhy       (QHYCCD ARM64 SDK required)
  Canon EOS    -> oal.canon     (direct native OAL driver; linked libgphoto2 USB/PTP transport)
  Gemini EAF   -> oal.gemini    (direct 9600-baud MyFocuserPro2 serial protocol)
  Sky-Watcher  -> oal.skywatcher (direct SynScan serial protocol v3.3)

INDI compatibility was: $([[ $WITH_INDI -eq 1 ]] && echo INSTALLED || echo NOT INSTALLED)
It can be enabled later with:
  sudo ./scripts/enable_indi_compat.sh
and compiled into OAL with either:
  cmake --preset rpi4-observatory-indi-release
or:
  cmake ... -DOAS_ENABLE_INDI=ON

The normal rpi4-observatory-release preset keeps INDI OFF and uses native OAL drivers by default.

Still vendor-specific:
  - install/stage the correct QHYCCD ARM64 SDK and udev rules;
  - Canon EOS requires USB access through the system libgphoto2/udev rules; close desktop auto-import/photo applications that claim the camera;
  - install ASTAP + one compatible star database.

Recommended deterministic serial binding on a permanent telescope:
  export OAL_GEMINI_PORT=<Gemini serial device>
  export OAL_SKYWATCHER_PORT=<SynScan serial device>
Put these in /etc/openastrolink/node.env after hardware discovery.

User '$OAL_USER' was added to available dialout/plugdev/video groups.
Log out/reboot for new group membership to take effect.
MSG
