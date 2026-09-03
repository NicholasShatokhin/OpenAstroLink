#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"

arch="${1:-arm64}"
[[ $# -gt 0 ]] && shift || true
case "$arch" in
  arm64|aarch64) arch="arm64"; default_preset="rpi4-cross-arm64-node-release" ;;
  armhf|arm32)   arch="armhf"; default_preset="rpi-cross-armhf-node-release" ;;
  *) echo "Usage: $0 [arm64|armhf] [SYSROOT] [PRESET] [--bootstrap [bootstrap options...]]" >&2; exit 2 ;;
esac

bootstrap=0
sysroot=""
preset=""
qt_host=""
qt_target_dir=""
opencv_target_dir=""
qhy_sdk=""
extra_bootstrap=()

# Backward compatible positional SYSROOT/PRESET, plus a convenient --bootstrap mode.
while [[ $# -gt 0 ]]; do
  case "$1" in
    --bootstrap) bootstrap=1; shift; extra_bootstrap=("$@"); break ;;
    --sysroot) sysroot="${2:?--sysroot requires PATH}"; shift 2 ;;
    --preset) preset="${2:?--preset requires NAME}"; shift 2 ;;
    --qt-host) qt_host="${2:?--qt-host requires PATH}"; shift 2 ;;
    --*) echo "Unknown option: $1" >&2; exit 2 ;;
    *)
      if [[ -z "$sysroot" ]]; then sysroot="$1";
      elif [[ -z "$preset" ]]; then preset="$1";
      else echo "Unexpected argument: $1" >&2; exit 2; fi
      shift
      ;;
  esac
done

if (( bootstrap )); then
  "$root/scripts/bootstrap_rpi_cross.sh" "$arch" "${extra_bootstrap[@]}"
fi

env_file="$root/.oal/rpi-cross-${arch}.env"
if [[ -f "$env_file" ]]; then
  # shellcheck disable=SC1090
  source "$env_file"
fi
sysroot="${sysroot:-${OAS_CROSS_SYSROOT:-}}"
qt_host="${qt_host:-${OAS_QT_HOST_PATH:-}}"
qt_target_dir="${OAS_QT_TARGET_DIR:-}"
opencv_target_dir="${OAS_OPENCV_TARGET_DIR:-}"
qhy_sdk="${OAS_QHY_SDK:-}"
preset="${preset:-$default_preset}"

if [[ -z "$sysroot" || ! -d "$sysroot" ]]; then
  echo "ERROR: target sysroot is missing." >&2
  echo "Create it automatically with:" >&2
  echo "  ./scripts/bootstrap_rpi_cross.sh $arch" >&2
  echo "or mirror a real Pi:" >&2
  echo "  ./scripts/bootstrap_rpi_cross.sh $arch --from-pi pi@openastrolink.local" >&2
  exit 3
fi

"$root/scripts/check_build_environment.sh"
echo "Cross preset: $preset"
echo "Target sysroot: $sysroot"
args=(--preset "$preset" -DOAS_CROSS_SYSROOT="$sysroot")
if [[ -n "$qt_host" ]]; then
  echo "Qt host tools: $qt_host"
  args+=(-DOAS_QT_HOST_PATH="$qt_host")
fi
if [[ -n "$qt_target_dir" ]]; then
  echo "Qt target package: $qt_target_dir"
  args+=(-DQt6_DIR="$qt_target_dir")
fi
if [[ -n "$opencv_target_dir" ]]; then
  echo "OpenCV target package: $opencv_target_dir"
  args+=(-DOpenCV_DIR="$opencv_target_dir")
fi
if [[ -n "$qhy_sdk" ]]; then
  qhy_include="$qhy_sdk/include/qhyccd.h"
  qhy_library=""
  for candidate in \
      "$qhy_sdk/lib/libqhy.so" \
      "$qhy_sdk/lib/libqhyccd.so" \
      "$qhy_sdk/lib/libqhyccd.a" \
      "$qhy_sdk/lib/libqhy.a"; do
    if [[ -e "$candidate" ]]; then qhy_library="$candidate"; break; fi
  done
  if [[ ! -f "$qhy_include" || -z "$qhy_library" ]]; then
    if [[ "$preset" == *full* ]]; then
      echo "ERROR: QHY SDK record is stale or incomplete: $qhy_sdk" >&2
      echo "Restage a matching SDK with:" >&2
      echo "  ./scripts/stage_qhy_cross_sdk.sh $arch /path/to/qhy-sdk" >&2
      exit 4
    fi
    echo "WARNING: ignoring stale QHY SDK record for non-full preset: $qhy_sdk" >&2
    qhy_sdk=""
  else
    echo "QHY target SDK: $qhy_sdk"
    echo "QHY target library: $qhy_library"
    args+=(-DQHYCCD_INCLUDE_DIR="$qhy_sdk/include" -DQHYCCD_LIBRARY="$qhy_library")
  fi
fi
if [[ -z "$qhy_sdk" && "$preset" == *full* ]]; then
  echo "ERROR: preset '$preset' enables the full vendor stack but no staged QHY SDK was recorded." >&2
  echo "Use a genuine target-architecture QHY SDK or select a non-full preset." >&2
  exit 4
fi
cmake "${args[@]}"
cmake --build --preset "$preset" -j"$(nproc 2>/dev/null || echo 4)"
