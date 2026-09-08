#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
qt_version="${OAL_NATIVE_QT_VERSION:-6.8.3}"
qhy_version="${OAL_QHY_VERSION:-26.06.04}"
with_vendor=1
install_system=1
require_canon=0
with_webrtc=1
webrtc_version="${OAL_LIBDATACHANNEL_VERSION:-0.24.5}"

usage() {
  cat <<USAGE
Usage: $0 [--qt-version X.Y.Z] [--qhy-version YY.MM.DD]
          [--no-vendor] [--no-system-packages] [--no-webrtc] [--require-canon]

Finds or installs the host-native dependencies used by OpenAstroLink.
Linux: apt for toolchain/OpenCV/system libraries + per-user Qt via aqt when the
       distro Qt is too old + QHY/ZWO vendor SDK bootstrap.
macOS: Homebrew for OpenCV/build tools when available + per-user Qt via aqt +
       QHY/ZWO vendor SDK bootstrap.
Canon EDSDK is auto-discovered but never downloaded because its distribution is
license-gated by Canon.

Writes .oal/native-deps-<platform>-<arch>.env for build_linux.sh/build_macos.sh.
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --qt-version) qt_version="$2"; shift 2 ;;
    --qhy-version) qhy_version="$2"; shift 2 ;;
    --no-vendor) with_vendor=0; shift ;;
    --no-system-packages) install_system=0; shift ;;
    --no-webrtc) with_webrtc=0; shift ;;
    --require-canon) require_canon=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
  esac
done

os="$(uname -s)"
arch="$(uname -m)"
case "$os" in
  Linux) platform="linux" ;;
  Darwin) platform="macos" ;;
  *) echo "ERROR: use bootstrap_native_dependencies.ps1 on Windows." >&2; exit 3 ;;
esac
case "$arch" in
  x86_64|amd64) norm_arch="x86_64" ;;
  aarch64|arm64) norm_arch="arm64" ;;
  *) echo "ERROR: unsupported native architecture: $arch" >&2; exit 4 ;;
esac

if [[ "$platform" == linux && $install_system -eq 1 ]]; then
  if command -v apt-get >/dev/null 2>&1; then
    packages=(
      build-essential cmake ninja-build pkg-config git curl ca-certificates
      python3 python3-venv python3-pip
      libgl1-mesa-dev libegl1-mesa-dev libxkbcommon-dev libxkbcommon-x11-dev
      libopencv-dev libusb-1.0-0-dev libjpeg-dev libssl-dev
    )
    missing=()
    for p in "${packages[@]}"; do dpkg-query -W -f='${Status}' "$p" 2>/dev/null | grep -q 'install ok installed' || missing+=("$p"); done
    if [[ ${#missing[@]} -gt 0 ]]; then
      echo "Installing missing native Linux build dependencies: ${missing[*]}"
      # Do not run apt-get update unconditionally.  An unrelated broken PPA can
      # make `apt-get update` fail even though Ubuntu's cached package indexes
      # are perfectly sufficient for the packages OAL needs.  Try installation
      # from the current indexes first; update only as a fallback.
      if ! sudo apt-get -o DPkg::Lock::Timeout=120 install -y "${missing[@]}"; then
        echo "Initial apt install failed; refreshing package indexes and retrying ..." >&2
        if ! sudo apt-get -o DPkg::Lock::Timeout=120 update; then
          cat >&2 <<'APTERR'
ERROR: apt-get update failed. This is often caused by an unrelated disabled or
broken third-party PPA. OpenAstroLink will not disable or rewrite your system
repositories automatically. Fix/remove the failing repository, or install the
packages printed above manually, then rerun this bootstrap.
APTERR
          exit 9
        fi
        sudo apt-get -o DPkg::Lock::Timeout=120 install -y "${missing[@]}"
      fi
    else
      echo "Native Linux system packages already present."
    fi
  else
    echo "WARNING: apt-get not found; system dependency installation is skipped." >&2
  fi
elif [[ "$platform" == macos && $install_system -eq 1 ]]; then
  if command -v brew >/dev/null 2>&1; then
    for f in cmake ninja pkg-config opencv; do
      if ! brew list --versions "$f" >/dev/null 2>&1; then
        echo "Installing Homebrew dependency: $f"
        brew install "$f"
      fi
    done
  else
    echo "WARNING: Homebrew is not installed. Existing CMake/Ninja/OpenCV will be used if discoverable." >&2
  fi
fi

"$root/scripts/bootstrap_qt_native.sh" --version "$qt_version"
qt_env="$root/.oal/native-qt-${platform}-${norm_arch}.env"
[[ -f "$qt_env" ]] || { echo "ERROR: Qt bootstrap did not create $qt_env" >&2; exit 5; }
# shellcheck disable=SC1090
source "$qt_env"

# Locate a CMake OpenCV package.  Prefer the host package manager's target.
opencv_dir=""
if [[ -n "${OpenCV_DIR:-}" && -f "${OpenCV_DIR}/OpenCVConfig.cmake" ]]; then
  opencv_dir="$OpenCV_DIR"
fi
if [[ -z "$opencv_dir" && "$platform" == linux ]]; then
  multi="$(gcc -print-multiarch 2>/dev/null || true)"
  for p in "/usr/lib/$multi/cmake/opencv4" "/usr/lib/cmake/opencv4" "/usr/local/lib/cmake/opencv4"; do
    [[ -f "$p/OpenCVConfig.cmake" ]] && { opencv_dir="$p"; break; }
  done
fi
if [[ -z "$opencv_dir" && "$platform" == macos ]] && command -v brew >/dev/null 2>&1; then
  op="$(brew --prefix opencv 2>/dev/null || true)"
  for p in "$op/lib/cmake/opencv4" "$op/share/opencv4"; do
    [[ -f "$p/OpenCVConfig.cmake" ]] && { opencv_dir="$p"; break; }
  done
fi
if [[ -z "$opencv_dir" ]]; then
  cfg="$(find /usr /usr/local /opt "$HOME/.local" -type f -name OpenCVConfig.cmake -path '*/opencv4/*' -print -quit 2>/dev/null || true)"
  [[ -n "$cfg" ]] && opencv_dir="$(dirname "$cfg")"
fi
[[ -n "$opencv_dir" ]] || { echo "ERROR: OpenCV >= 4 CMake package was not found after bootstrap." >&2; exit 6; }
echo "OpenCV package: $opencv_dir"

# WebRTC preview transport: prefer an existing libdatachannel package, otherwise
# build the pinned release per-user. Media/WebSocket helpers are disabled because
# OAL uses Qt for signaling and OALV/JPEG DataChannels for this checkpoint.
libdatachannel_dir=""
if [[ -n "${LibDataChannel_DIR:-}" && -d "${LibDataChannel_DIR}" ]]; then
  cfg="$(find "${LibDataChannel_DIR}" -maxdepth 1 -type f -iname '*config.cmake' -print -quit 2>/dev/null || true)"
  [[ -n "$cfg" ]] && libdatachannel_dir="${LibDataChannel_DIR}"
fi
if [[ -z "$libdatachannel_dir" ]]; then
  cfg="$(find /usr /usr/local "$HOME/.local" -type f \( -iname 'LibDataChannelConfig.cmake' -o -iname 'libdatachannel-config.cmake' \) -print -quit 2>/dev/null || true)"
  [[ -n "$cfg" ]] && libdatachannel_dir="$(dirname "$cfg")"
fi
if [[ $with_webrtc -eq 1 && -z "$libdatachannel_dir" ]]; then
  webrtc_root="$HOME/.local/share/openastrolink/libdatachannel/$webrtc_version/$norm_arch"
  webrtc_src="$webrtc_root/src"
  webrtc_build="$webrtc_root/build"
  webrtc_prefix="$webrtc_root/install"
  mkdir -p "$webrtc_root"
  if [[ ! -f "$webrtc_src/CMakeLists.txt" ]]; then
    rm -rf "$webrtc_src"
    echo "Cloning libdatachannel v$webrtc_version for WebRTC preview ..."
    git clone --depth 1 --branch "v$webrtc_version" --recurse-submodules --shallow-submodules       https://github.com/paullouisageneau/libdatachannel.git "$webrtc_src"
  fi
  echo "Building per-user libdatachannel v$webrtc_version ..."
  cmake -S "$webrtc_src" -B "$webrtc_build" -G Ninja     -DCMAKE_BUILD_TYPE=Release     -DCMAKE_INSTALL_PREFIX="$webrtc_prefix"     -DNO_MEDIA=ON -DNO_WEBSOCKET=ON -DNO_EXAMPLES=ON -DBUILD_TESTING=OFF     -DBUILD_SHARED_LIBS=OFF
  cmake --build "$webrtc_build" --parallel "$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
  cmake --install "$webrtc_build"
  cfg="$(find "$webrtc_prefix" -type f \( -iname 'LibDataChannelConfig.cmake' -o -iname 'libdatachannel-config.cmake' \) -print -quit 2>/dev/null || true)"
  [[ -n "$cfg" ]] || { echo "ERROR: libdatachannel install completed but no CMake package was found." >&2; exit 10; }
  libdatachannel_dir="$(dirname "$cfg")"
fi
if [[ $with_webrtc -eq 1 && -z "$libdatachannel_dir" ]]; then
  echo "ERROR: WebRTC was requested but libdatachannel could not be located." >&2
  exit 10
fi
if [[ -n "$libdatachannel_dir" ]]; then
  echo "libdatachannel package: $libdatachannel_dir"
fi

vendor_env=""
if [[ $with_vendor -eq 1 ]]; then
  "$root/scripts/bootstrap_vendor_sdks.sh" --qhy-version "$qhy_version"
  vendor_env="$root/.oal/native-vendor-${platform}-${norm_arch}.env"
  [[ -f "$vendor_env" ]] || { echo "ERROR: vendor bootstrap did not create $vendor_env" >&2; exit 7; }
fi

canon_env="$root/.oal/native-canon-${platform}-${norm_arch}.env"
if "$root/scripts/discover_canon_edsdk.sh"; then
  :
else
  rm -f "$canon_env"
  if [[ $require_canon -eq 1 ]]; then
    echo "ERROR: Canon EDSDK is required for this build but was not found." >&2
    exit 8
  fi
fi

mkdir -p "$root/.oal"
out="$root/.oal/native-deps-${platform}-${norm_arch}.env"
: > "$out"
printf 'OAS_QT_ROOT=%q\nCMAKE_PREFIX_PATH=%q\nOAS_QT_VERSION=%q\nOpenCV_DIR=%q\n' \
  "$OAS_QT_ROOT" "$CMAKE_PREFIX_PATH" "$OAS_QT_VERSION" "$opencv_dir" >> "$out"
[[ -n "$libdatachannel_dir" ]] && printf 'LibDataChannel_DIR=%q\n' "$libdatachannel_dir" >> "$out"
[[ -n "$vendor_env" && -f "$vendor_env" ]] && cat "$vendor_env" >> "$out"
[[ -f "$canon_env" ]] && cat "$canon_env" >> "$out"

cat <<READY
Native OpenAstroLink dependency environment is ready.
  host:       $platform/$norm_arch
  Qt:         $OAS_QT_VERSION ($OAS_QT_ROOT)
  OpenCV:     $opencv_dir
  WebRTC:     ${libdatachannel_dir:-disabled}
  env record: $out
READY
if [[ -f "$canon_env" ]]; then
  echo "  Canon:      EDSDK discovered"
else
  echo "  Canon:      not found (manual Canon SDK required for EDSDK presets)"
fi
