#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
arch="${1:-arm64}"
version="${2:-26.06.04}"
dest="${3:-}"
cache_dir="${OAL_QHY_CACHE_DIR:-$HOME/.cache/openastrolink/qhyccd}"

case "$arch" in
  arm64|aarch64)
    arch="arm64"
    dest="${dest:-$HOME/.local/share/openastrolink/sdk/qhy-arm64}"
    ;;
  armhf|arm32)
    arch="armhf"
    dest="${dest:-$HOME/.local/share/openastrolink/sdk/qhy-armhf}"
    ;;
  x86_64|amd64)
    arch="x86_64"
    dest="${dest:-$HOME/.local/share/openastrolink/sdk/qhy-x86_64}"
    ;;
  *)
    echo "Usage: $0 [arm64|armhf|x86_64] [VERSION] [DEST]" >&2
    exit 2
    ;;
esac

if ! [[ "$version" =~ ^[0-9]{2}\.[0-9]{2}\.[0-9]{2}$ ]]; then
  echo "ERROR: QHY SDK version must use YY.MM.DD form (for example 26.06.04): $version" >&2
  exit 2
fi

IFS=. read -r vy vm vd <<<"$version"
key=$((10#$vy * 10000 + 10#$vm * 100 + 10#$vd))
if (( key >= 260604 )); then
  scheme="new"
  repo_dir="${version//./}"
  case "$arch" in
    arm64)  file="sdk_linux_arm64_${version}.tar.gz" ;;
    x86_64) file="sdk_linux64_${version}.tar.gz" ;;
    armhf)
      echo "ERROR: QHY's >=26.06.04 public naming scheme documents linux_arm64 and linux64 packages, but no ARMHF filename." >&2
      echo "Use a known ARMHF archive explicitly with stage_qhy_cross_sdk.sh." >&2
      exit 3
      ;;
  esac
else
  scheme="legacy"
  repo_dir="$version"
  case "$arch" in
    arm64)  file="sdk_Arm64_${version}.tgz" ;;
    x86_64) file="sdk_linux64_${version}.tgz" ;;
    armhf)  file="sdk_arm32_${version}.tgz" ;;
  esac
fi

url="https://www.qhyccd.com/file/repository/publish/SDK/${repo_dir}/${file}"
mkdir -p "$cache_dir"
archive="$cache_dir/$file"

if [[ -s "$archive" ]]; then
  echo "Using cached QHY SDK archive: $archive"
else
  echo "Downloading official QHYCCD SDK $version ($arch, $scheme packaging) ..."
  echo "  $url"
  tmp="$archive.part"
  rm -f "$tmp"
  if command -v curl >/dev/null 2>&1; then
    curl --fail --location --retry 3 --retry-delay 2 --connect-timeout 20 \
      --output "$tmp" "$url"
  elif command -v wget >/dev/null 2>&1; then
    wget --tries=3 --timeout=30 -O "$tmp" "$url"
  else
    echo "ERROR: curl or wget is required to download the QHY SDK." >&2
    exit 4
  fi
  [[ -s "$tmp" ]] || { echo "ERROR: QHY download produced an empty file." >&2; exit 5; }
  mv "$tmp" "$archive"
fi

# Verify that the payload is actually a gzip tarball before passing it to the
# architecture-aware stager.  The stager does not trust the filename either.
tar -tzf "$archive" >/dev/null || {
  echo "ERROR: downloaded QHY file is not a valid gzip tar archive: $archive" >&2
  rm -f "$archive"
  exit 6
}

"$root/scripts/stage_qhy_cross_sdk.sh" "$arch" "$archive" "$dest"

cat > "$dest/OAL_QHY_OFFICIAL_SOURCE.txt" <<META
vendor=QHYCCD
version=$version
packaging=$scheme
url=$url
archive=$archive
target_arch=$arch
META

echo "Official QHYCCD SDK ready:"
echo "  version: $version"
echo "  target:  $arch"
echo "  staged:  $dest"
