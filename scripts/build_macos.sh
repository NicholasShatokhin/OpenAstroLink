#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"

preset="my-macos-arm64-observatory"
auto_deps=1
force_deps=0
qhy_version="26.06.04"
qt_version="6.8.3"

for arg in "$@"; do
  case "$arg" in
    --no-auto-deps) auto_deps=0 ;;
    --bootstrap-deps|--bootstrap-vendor) auto_deps=1; force_deps=1 ;;
    --qt-version=*) qt_version="${arg#*=}" ;;
    --qhy-version=*) qhy_version="${arg#*=}" ;;
    --use-vendor-sdk) auto_deps=1 ;;
    --*) echo "Unknown option: $arg" >&2; exit 2 ;;
    *) preset="$arg" ;;
  esac
done

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "ERROR: build_macos.sh must run on macOS (Darwin)." >&2
  exit 2
fi
if ! command -v c++ >/dev/null 2>&1; then echo "ERROR: Apple Clang/C++ compiler not found. Install Xcode Command Line Tools." >&2; exit 4; fi

norm=x86_64
[[ "$(uname -m)" == arm64 ]] && norm=arm64
cmake_args=(--preset "$preset")

if [[ $auto_deps -eq 1 ]]; then
  dep_env="$root/.oal/native-deps-macos-${norm}.env"
  need_bootstrap=$force_deps
  [[ -f "$dep_env" ]] || need_bootstrap=1
  if [[ $need_bootstrap -eq 0 ]]; then
    # shellcheck disable=SC1090
    source "$dep_env"
    [[ -f "${OAS_QT_ROOT:-}/lib/cmake/Qt6/Qt6Config.cmake" ]] || need_bootstrap=1
    [[ -f "${OpenCV_DIR:-}/OpenCVConfig.cmake" ]] || need_bootstrap=1
    [[ -f "${QHYCCD_INCLUDE_DIR:-}/qhyccd.h" ]] || need_bootstrap=1
    [[ -f "${ZWO_ASI_INCLUDE_DIR:-}/ASICamera2.h" ]] || need_bootstrap=1
    [[ -f "${ZWO_EAF_INCLUDE_DIR:-}/EAF_focuser.h" ]] || need_bootstrap=1
  fi
  if [[ $need_bootstrap -eq 1 ]]; then
    "$root/scripts/bootstrap_native_dependencies.sh" \
      --qt-version "$qt_version" --qhy-version "$qhy_version"
  fi
  # shellcheck disable=SC1090
  source "$dep_env"
  for name in CMAKE_PREFIX_PATH OpenCV_DIR QHYCCD_INCLUDE_DIR QHYCCD_LIBRARY QHYCCD_RUNTIME_DIR QHYCCD_ROOT ZWO_ASI_INCLUDE_DIR ZWO_ASI_LIBRARY ZWO_ASI_RUNTIME_DIR ZWO_ASI_ROOT ZWO_EAF_INCLUDE_DIR ZWO_EAF_LIBRARY ZWO_EAF_RUNTIME_DIR ZWO_EAF_ROOT CANON_EDSDK_INCLUDE_DIR CANON_EDSDK_LIBRARY CANON_EDSDK_RUNTIME_DIR CANON_EDSDK_ROOT; do
    if [[ -n "${!name:-}" ]]; then cmake_args+=("-D${name}=${!name}"); fi
  done
  cmake_args+=("-DOAS_ENABLE_QHY=ON")
fi

command -v cmake >/dev/null 2>&1 || { echo "ERROR: cmake not found after dependency bootstrap." >&2; exit 3; }
echo "Host:  $(uname -s) $(uname -m)"
echo "CMake: $(cmake --version | head -n1)"
echo "C++:   $(c++ --version | head -n1)"
cmake "${cmake_args[@]}"
cmake --build --preset "$preset" -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
