#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
qhy_version="26.06.04"
with_qhy=1
with_zwo=1
dest_root="${OAL_VENDOR_SDK_ROOT:-$HOME/.local/share/openastrolink/sdk/native}"
zwo_commit="${OAL_ZWO_INDI_COMMIT:-b0802f2}"

usage() {
  cat <<EOF
Usage: $0 [--qhy-version YY.MM.DD] [--no-qhy] [--no-zwo] [--dest-root DIR]

Bootstraps host-native camera SDKs without modifying /usr/local:
  * QHYCCD: official QHY SDK archive (Linux/macOS)
  * ZWO ASI/EAF: pinned INDI mirror of ZWO's MIT SDK blobs

The generated env record can be consumed by build_linux.sh/build_macos.sh.
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --qhy-version) qhy_version="$2"; shift 2 ;;
    --no-qhy) with_qhy=0; shift ;;
    --no-zwo) with_zwo=0; shift ;;
    --dest-root) dest_root="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
  esac
done

os="$(uname -s)"
arch="$(uname -m)"
case "$os" in
  Linux) platform="linux" ;;
  Darwin) platform="macos" ;;
  *) echo "ERROR: bootstrap_vendor_sdks.sh supports Linux and macOS. Use bootstrap_vendor_sdks.ps1 on Windows." >&2; exit 3 ;;
esac

case "$arch" in
  x86_64|amd64) norm_arch="x86_64" ;;
  aarch64|arm64) norm_arch="arm64" ;;
  *) echo "ERROR: unsupported native architecture: $arch" >&2; exit 4 ;;
esac

stage="$dest_root/${platform}-${norm_arch}"
mkdir -p "$stage" "$root/.oal"
env_file="$root/.oal/native-vendor-${platform}-${norm_arch}.env"
: > "$env_file"

quote_env() { printf '%q' "$1"; }
write_env() { printf '%s=%s\n' "$1" "$(quote_env "$2")" >> "$env_file"; }

fetch() {
  local url="$1" out="$2"
  mkdir -p "$(dirname "$out")"
  if [[ -s "$out" ]]; then
    echo "Using cached: $out"
    return
  fi
  echo "Downloading: $url"
  local tmp="$out.part"
  rm -f "$tmp"
  if command -v curl >/dev/null 2>&1; then
    curl --fail --location --retry 3 --retry-delay 2 --connect-timeout 20 -o "$tmp" "$url"
  elif command -v wget >/dev/null 2>&1; then
    wget --tries=3 --timeout=30 -O "$tmp" "$url"
  else
    echo "ERROR: curl or wget is required." >&2; exit 5
  fi
  mv "$tmp" "$out"
}

if [[ $with_qhy -eq 1 ]]; then
  qhy_stage="$stage/qhy"
  if [[ "$platform" == linux ]]; then
    qhy_target="$norm_arch"
    "$root/scripts/fetch_qhy_sdk.sh" "$qhy_target" "$qhy_version" "$qhy_stage"
    qhy_lib="$(find "$qhy_stage/lib" -maxdepth 1 \( -type f -o -type l \) \( -name 'libqhy.so' -o -name 'libqhyccd.so' -o -name 'libqhyccd.a' \) -print -quit)"
  else
    IFS=. read -r vy vm vd <<<"$qhy_version"
    key=$((10#$vy * 10000 + 10#$vm * 100 + 10#$vd))
    if (( key >= 260604 )); then
      repo_dir="${qhy_version//./}"
      [[ "$norm_arch" == arm64 ]] && qhy_file="sdk_mac_arm_${qhy_version}.tar.gz" || qhy_file="sdk_mac_x64_${qhy_version}.tar.gz"
    else
      repo_dir="$qhy_version"
      [[ "$norm_arch" == arm64 ]] && qhy_file="sdk_mac_arm_${qhy_version}.tgz" || qhy_file="sdk_macMix_${qhy_version}.tgz"
    fi
    cache="${OAL_QHY_CACHE_DIR:-$HOME/.cache/openastrolink/qhyccd}/$qhy_file"
    fetch "https://www.qhyccd.com/file/repository/publish/SDK/${repo_dir}/${qhy_file}" "$cache"
    tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
    tar -xzf "$cache" -C "$tmp"
    hdr="$(find "$tmp" -type f -name qhyccd.h -print -quit)"
    [[ -n "$hdr" ]] || { echo "ERROR: qhyccd.h not found in $cache" >&2; exit 6; }
    # Prefer the shared library; fall back to static if the vendor package changes.
    qhy_src="$(find "$tmp" -type f \( -name 'libqhyccd*.dylib' -o -name 'libqhy*.dylib' \) -print -quit)"
    [[ -n "$qhy_src" ]] || qhy_src="$(find "$tmp" -type f \( -name 'libqhyccd.a' -o -name 'libqhy.a' \) -print -quit)"
    [[ -n "$qhy_src" ]] || { echo "ERROR: QHY macOS library not found in $cache" >&2; exit 7; }
    desc="$(file "$qhy_src")"
    if [[ "$norm_arch" == arm64 ]]; then grep -Eqi 'Mach-O.*arm64|arm64' <<<"$desc" || { echo "ERROR: QHY library is not macOS arm64: $desc" >&2; exit 8; }
    else grep -Eqi 'Mach-O.*x86_64|x86_64' <<<"$desc" || { echo "ERROR: QHY library is not macOS x86_64: $desc" >&2; exit 8; }; fi
    rm -rf "$qhy_stage"; mkdir -p "$qhy_stage/include" "$qhy_stage/lib"
    cp -a "$(dirname "$hdr")"/*.h "$qhy_stage/include/"
    ext="${qhy_src##*.}"
    if [[ "$ext" == dylib ]]; then qhy_lib="$qhy_stage/lib/libqhyccd.dylib"; else qhy_lib="$qhy_stage/lib/libqhyccd.a"; fi
    cp -a "$qhy_src" "$qhy_lib"
    if [[ "$qhy_lib" == *.dylib ]] && command -v install_name_tool >/dev/null 2>&1; then
      install_name_tool -id "@rpath/$(basename "$qhy_lib")" "$qhy_lib" || true
    fi
    echo "QHY SDK staged: $qhy_stage"
    echo "  library: $qhy_lib"
  fi
  write_env QHYCCD_INCLUDE_DIR "$qhy_stage/include"
  write_env QHYCCD_LIBRARY "$qhy_lib"
  write_env QHYCCD_ROOT "$qhy_stage"
  if [[ "$platform" == macos ]]; then write_env QHYCCD_RUNTIME_DIR "$qhy_stage/lib"; fi
fi

if [[ $with_zwo -eq 1 ]]; then
  zwo_stage="$stage/zwo"
  asi_root="$zwo_stage/asi"; eaf_root="$zwo_stage/eaf"
  mkdir -p "$asi_root/include" "$asi_root/lib" "$eaf_root/include" "$eaf_root/lib"
  base="https://raw.githubusercontent.com/indilib/indi-3rdparty/${zwo_commit}/libasi"
  cache="${OAL_ZWO_CACHE_DIR:-$HOME/.cache/openastrolink/zwo}/${zwo_commit}"
  fetch "$base/ASICamera2.h" "$cache/ASICamera2.h"
  fetch "$base/EAF_focuser.h" "$cache/EAF_focuser.h"
  fetch "$base/license.txt" "$cache/license.txt"
  if [[ "$platform" == linux ]]; then
    [[ "$norm_arch" == arm64 ]] && zwo_arch="armv8" || zwo_arch="x64"
    asi_name="libASICamera2.so"; eaf_name="libEAFFocuser.so"
  else
    [[ "$norm_arch" == arm64 ]] && zwo_arch="mac_arm64" || zwo_arch="mac_x64"
    asi_name="libASICamera2.dylib"; eaf_name="libEAFFocuser.dylib"
  fi
  fetch "$base/$zwo_arch/libASICamera2.bin" "$cache/${zwo_arch}-libASICamera2.bin"
  fetch "$base/$zwo_arch/libEAFFocuser.bin" "$cache/${zwo_arch}-libEAFFocuser.bin"
  cp -a "$cache/ASICamera2.h" "$cache/license.txt" "$asi_root/include/"
  cp -a "$cache/EAF_focuser.h" "$cache/license.txt" "$eaf_root/include/"
  cp -a "$cache/${zwo_arch}-libASICamera2.bin" "$asi_root/lib/$asi_name"
  cp -a "$cache/${zwo_arch}-libEAFFocuser.bin" "$eaf_root/lib/$eaf_name"
  chmod 0755 "$asi_root/lib/$asi_name" "$eaf_root/lib/$eaf_name" || true
  if [[ "$platform" == macos ]] && command -v install_name_tool >/dev/null 2>&1; then
    install_name_tool -id "@rpath/$asi_name" "$asi_root/lib/$asi_name" || true
    install_name_tool -id "@rpath/$eaf_name" "$eaf_root/lib/$eaf_name" || true
  fi
  echo "ZWO SDK staged from pinned INDI mirror commit $zwo_commit:"
  echo "  ASI: $asi_root/lib/$asi_name ($(file "$asi_root/lib/$asi_name"))"
  echo "  EAF: $eaf_root/lib/$eaf_name ($(file "$eaf_root/lib/$eaf_name"))"
  write_env ZWO_ASI_INCLUDE_DIR "$asi_root/include"
  write_env ZWO_ASI_LIBRARY "$asi_root/lib/$asi_name"
  write_env ZWO_ASI_ROOT "$asi_root"
  write_env ZWO_EAF_INCLUDE_DIR "$eaf_root/include"
  write_env ZWO_EAF_LIBRARY "$eaf_root/lib/$eaf_name"
  write_env ZWO_EAF_ROOT "$eaf_root"
  write_env ZWO_ASI_RUNTIME_DIR "$asi_root/lib"
  write_env ZWO_EAF_RUNTIME_DIR "$eaf_root/lib"
fi

cat <<EOF
Native vendor SDK environment is ready.
  host:       $platform/$norm_arch
  SDK root:   $stage
  env record: $env_file

Use:
  ./scripts/build_${platform/macos/macos}.sh <preset> --use-vendor-sdk
EOF
