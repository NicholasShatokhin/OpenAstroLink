#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"

preset="my-linux-observatory"
auto_deps=1
force_deps=0
clean_build=0
qhy_version="26.06.04"
qt_version="6.8.3"

usage() {
  cat <<USAGE
Usage: $0 [preset] [--no-auto-deps] [--bootstrap-deps] [--clean]
          [--qt-version=X.Y.Z] [--qhy-version=YY.MM.DD]

By default the wrapper searches for all native dependencies first and downloads
or installs the redistributable ones when missing.  Canon EDSDK is searched
locally but is never downloaded automatically.
USAGE
}

for arg in "$@"; do
  case "$arg" in
    --no-auto-deps) auto_deps=0 ;;
    --bootstrap-deps|--bootstrap-vendor) auto_deps=1; force_deps=1 ;;
    --clean) clean_build=1 ;;
    --qt-version=*) qt_version="${arg#*=}" ;;
    --qhy-version=*) qhy_version="${arg#*=}" ;;
    -h|--help) usage; exit 0 ;;
    --*) echo "Unknown option: $arg" >&2; exit 2 ;;
    *) preset="$arg" ;;
  esac
done

cmake_args=(--preset "$preset")

# Native and WSL checkouts are often moved between /mnt/c/... and ~/workspace/....
# A copied CMakeCache.txt embeds the old absolute source path and cannot be reused.
# Clean only the known build directory for this preset when the cache belongs to a
# different checkout, or when --clean was explicitly requested.
build_dir=""
case "$preset" in
  my-linux-observatory|linux-observatory-release) build_dir="$root/build/linux-observatory" ;;
  my-linux-observatory-indi|linux-observatory-indi-release) build_dir="$root/build/linux-observatory-indi" ;;
  linux-node-release) build_dir="$root/build/linux-node" ;;
  linux-node-indi-release) build_dir="$root/build/linux-node-indi" ;;
  linux-native-release) build_dir="$root/build/linux-native" ;;
esac
if [[ -n "$build_dir" && -f "$build_dir/CMakeCache.txt" ]]; then
  cached_home="$(sed -n 's#^CMAKE_HOME_DIRECTORY:INTERNAL=##p' "$build_dir/CMakeCache.txt" | head -n1)"
  if [[ $clean_build -eq 1 || ( -n "$cached_home" && "$cached_home" != "$root" ) ]]; then
    if [[ -n "$cached_home" && "$cached_home" != "$root" ]]; then
      echo "Removing stale CMake cache copied from: $cached_home"
    else
      echo "Cleaning Linux build directory: $build_dir"
    fi
    rm -rf "$build_dir"
  fi
elif [[ $clean_build -eq 1 && -n "$build_dir" ]]; then
  rm -rf "$build_dir"
fi

if [[ $auto_deps -eq 1 ]]; then
  dep_env="$root/.oal/native-deps-linux-$(uname -m).env"
  [[ "$(uname -m)" == x86_64 ]] && dep_env="$root/.oal/native-deps-linux-x86_64.env"
  [[ "$(uname -m)" == aarch64 ]] && dep_env="$root/.oal/native-deps-linux-arm64.env"

  # A full bootstrap is idempotent and cached.  Run it when the record is absent
  # or explicitly requested.  Otherwise validate the recorded paths below and
  # refresh automatically if something was deleted/moved.
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
    echo "Bootstrapping native Linux dependencies (per-user Qt + vendor SDKs; system OpenCV/toolchain as needed) ..."
    "$root/scripts/bootstrap_native_dependencies.sh" \
      --qt-version "$qt_version" --qhy-version "$qhy_version"
  fi

  # shellcheck disable=SC1090
  source "$dep_env"
  for name in CMAKE_PREFIX_PATH OpenCV_DIR QHYCCD_INCLUDE_DIR QHYCCD_LIBRARY QHYCCD_RUNTIME_DIR QHYCCD_ROOT ZWO_ASI_INCLUDE_DIR ZWO_ASI_LIBRARY ZWO_ASI_RUNTIME_DIR ZWO_ASI_ROOT ZWO_EAF_INCLUDE_DIR ZWO_EAF_LIBRARY ZWO_EAF_RUNTIME_DIR ZWO_EAF_ROOT CANON_EDSDK_INCLUDE_DIR CANON_EDSDK_LIBRARY CANON_EDSDK_RUNTIME_DIR CANON_EDSDK_ROOT; do
    if [[ -n "${!name:-}" ]]; then cmake_args+=("-D${name}=${!name}"); fi
  done
fi

OAS_ALLOW_CUSTOM_QT=1 OAS_QT_ROOT="${OAS_QT_ROOT:-}" "$root/scripts/check_build_environment.sh"
cmake "${cmake_args[@]}"
cmake --build --preset "$preset" -j"$(nproc 2>/dev/null || echo 4)"
