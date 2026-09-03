#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
arch="arm64"
source_mode="debian"
pi_host=""
sysroot=""
qhy_source="$root/../QHYCCD_Linux_New"
qhy_version=""
qhy_dest=""
qhy_mode="auto"
with_indi=0
bootstrap_host_qt=1
recreate=0

usage() {
  cat <<USAGE
Usage:
  $0 [arm64|armhf] [options]

Creates a target sysroot for OAL Raspberry Pi cross-compilation. By default it
bootstraps a Debian 12/Bookworm target root locally. For the exact filesystem
of a real Raspberry Pi use --from-pi USER@HOST.

Options:
  --from-debian-bookworm       create/download a Debian 12 target root (default)
  --from-pi USER@HOST          install target deps over SSH and rsync that Pi
  --sysroot PATH               target sysroot directory
  --qhy-source PATH            QHY SDK archive or directory (default: ../QHYCCD_Linux_New)
  --qhy-version YY.MM.DD       download/stage official QHY SDK (e.g. 26.06.04)
  --qhy-repo PATH              compatibility alias for --qhy-source
  --require-qhy                fail unless a matching QHY SDK is staged
  --no-qhy                     do not stage the QHY ARM SDK
  --with-indi                  install INDI runtime packages in the target sysroot
  --skip-host-qt               do not bootstrap matching x86_64 Qt host tools
  --recreate                   remove an existing sysroot first
  -h, --help                   show this help

Examples:
  $0 arm64
  $0 arm64 --from-pi pi@openastrolink.local
  $0 arm64 --sysroot /opt/openastrolink-sysroots/rpi4-arm64
USAGE
}

if [[ $# -gt 0 && "$1" != --* ]]; then arch="$1"; shift; fi
case "$arch" in
  arm64|aarch64)
    arch="arm64"; deb_arch="arm64"; qemu="qemu-aarch64-static"; triple="aarch64-linux-gnu"
    default_sysroot="$HOME/.local/share/openastrolink/sysroots/rpi4-arm64"
    default_qhy="$HOME/.local/share/openastrolink/sdk/qhy-arm64"
    ;;
  armhf|arm32)
    arch="armhf"; deb_arch="armhf"; qemu="qemu-arm-static"; triple="arm-linux-gnueabihf"
    default_sysroot="$HOME/.local/share/openastrolink/sysroots/rpi-armhf"
    default_qhy="$HOME/.local/share/openastrolink/sdk/qhy-armhf"
    ;;
  *) echo "ERROR: unsupported architecture '$arch'" >&2; usage; exit 2 ;;
esac

while [[ $# -gt 0 ]]; do
  case "$1" in
    --from-debian-bookworm) source_mode="debian"; shift ;;
    --from-pi) source_mode="pi"; pi_host="${2:?--from-pi requires USER@HOST}"; shift 2 ;;
    --sysroot) sysroot="${2:?--sysroot requires PATH}"; shift 2 ;;
    --qhy-source) qhy_source="${2:?--qhy-source requires PATH}"; shift 2 ;;
    --qhy-version) qhy_version="${2:?--qhy-version requires YY.MM.DD}"; shift 2 ;;
    --qhy-repo) qhy_source="${2:?--qhy-repo requires PATH}"; shift 2 ;;
    --require-qhy) qhy_mode="required"; shift ;;
    --no-qhy) qhy_mode="off"; shift ;;
    --with-indi) with_indi=1; shift ;;
    --skip-host-qt) bootstrap_host_qt=0; shift ;;
    --recreate) recreate=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "ERROR: unknown option '$1'" >&2; usage; exit 2 ;;
  esac
done
sysroot="${sysroot:-$default_sysroot}"
qhy_dest="${qhy_dest:-$default_qhy}"

host_packages=(rsync symlinks file ca-certificates curl xz-utils "gcc-${triple}" "g++-${triple}")
if [[ "$source_mode" == "debian" ]]; then
  host_packages+=(debootstrap qemu-user-static binfmt-support debian-archive-keyring gnupg)
else
  host_packages+=(openssh-client)
fi

if command -v apt-get >/dev/null 2>&1; then
  missing=()
  for pkg in "${host_packages[@]}"; do dpkg -s "$pkg" >/dev/null 2>&1 || missing+=("$pkg"); done
  if (( ${#missing[@]} )); then
    echo "Installing host bootstrap packages: ${missing[*]}"
    sudo apt-get update
    sudo apt-get install -y "${missing[@]}"
  fi
else
  echo "ERROR: automatic host bootstrap currently supports Debian/Ubuntu/WSL hosts." >&2
  exit 3
fi

if (( recreate )) && [[ -e "$sysroot" ]]; then
  echo "Removing existing sysroot: $sysroot"
  sudo rm -rf "$sysroot"
fi
mkdir -p "$(dirname "$sysroot")"

target_packages=(
  ca-certificates libc6-dev pkg-config
  qt6-base-dev qt6-serialport-dev qt6-websockets-dev qt6-httpserver-dev qt6-positioning-dev
  libopencv-dev libusb-1.0-0-dev libjpeg-dev
  libblas3 liblapack3 libblas-dev liblapack-dev
)
if (( with_indi )); then target_packages+=(indi-bin); fi

if [[ "$source_mode" == "pi" ]]; then
  [[ -n "$pi_host" ]] || { echo "ERROR: --from-pi requires USER@HOST" >&2; exit 4; }
  remote_arch="$(ssh "$pi_host" uname -m)"
  if [[ "$arch" == arm64 && ! "$remote_arch" =~ ^(aarch64|arm64)$ ]]; then
    echo "ERROR: remote target reports '$remote_arch', expected ARM64." >&2; exit 5
  fi
  if [[ "$arch" == armhf && ! "$remote_arch" =~ ^(armv6|armv7|armv8l|arm)$ ]]; then
    echo "ERROR: remote target reports '$remote_arch', expected 32-bit ARM." >&2; exit 5
  fi

  echo "Installing target development packages on $pi_host ..."
  quoted="$(printf ' %q' "${target_packages[@]}")"
  ssh -t "$pi_host" "sudo apt-get update && sudo apt-get install -y$quoted"

  mkdir -p "$sysroot/lib" "$sysroot/usr/include" "$sysroot/usr/lib" "$sysroot/usr/share"
  echo "Synchronizing target filesystem into $sysroot ..."
  rsync -aH --delete "$pi_host:/lib/" "$sysroot/lib/"
  rsync -aH --delete "$pi_host:/usr/include/" "$sysroot/usr/include/"
  rsync -aH --delete "$pi_host:/usr/lib/" "$sysroot/usr/lib/"
  for share_dir in pkgconfig qt6 cmake; do
    if ssh "$pi_host" "test -d /usr/share/$share_dir"; then
      mkdir -p "$sysroot/usr/share/$share_dir"
      rsync -aH --delete "$pi_host:/usr/share/$share_dir/" "$sysroot/usr/share/$share_dir/"
    fi
  done
else
  # A local Bookworm root is reproducible and does not require a physical Pi.
  # Foreign-architecture chroots require a working binfmt_misc QEMU handler.
  # WSL does not always mount/register qemu-user-static automatically, so do it
  # explicitly and prove that an ARM binary executes before entering stage two.
  qemu_name="${qemu%-static}"
  marker="$sysroot/.oal-debootstrap-complete"

  ensure_binfmt() {
    if ! grep -qs ' /proc/sys/fs/binfmt_misc ' /proc/mounts; then
      echo "Mounting binfmt_misc ..."
      sudo mkdir -p /proc/sys/fs/binfmt_misc
      sudo mount -t binfmt_misc binfmt_misc /proc/sys/fs/binfmt_misc
    fi

    if [[ -w /proc/sys/fs/binfmt_misc/status ]] && [[ "$(cat /proc/sys/fs/binfmt_misc/status)" != "enabled" ]]; then
      echo 1 | sudo tee /proc/sys/fs/binfmt_misc/status >/dev/null
    fi

    if [[ ! -e "/proc/sys/fs/binfmt_misc/$qemu_name" ]]; then
      echo "Registering $qemu_name with binfmt_misc ..."
      if command -v update-binfmts >/dev/null 2>&1; then
        sudo update-binfmts --import "$qemu_name" >/dev/null 2>&1 || true
        sudo update-binfmts --enable "$qemu_name" >/dev/null 2>&1 || true
      fi
    fi

    if [[ ! -e "/proc/sys/fs/binfmt_misc/$qemu_name" ]]; then
      echo "ERROR: QEMU binfmt handler '$qemu_name' is not registered." >&2
      echo "On WSL try:" >&2
      echo "  sudo mount -t binfmt_misc binfmt_misc /proc/sys/fs/binfmt_misc" >&2
      echo "  sudo update-binfmts --import $qemu_name" >&2
      echo "  sudo update-binfmts --enable $qemu_name" >&2
      echo "Then rerun this script with --recreate if the sysroot is inconsistent." >&2
      exit 8
    fi

    if ! grep -q '^enabled' "/proc/sys/fs/binfmt_misc/$qemu_name"; then
      echo "ERROR: QEMU binfmt handler '$qemu_name' exists but is disabled." >&2
      cat "/proc/sys/fs/binfmt_misc/$qemu_name" >&2 || true
      exit 8
    fi
  }

  ensure_binfmt

  # Ubuntu 22.04 ships an old debian-archive-keyring (2021.x) that does not
  # contain the Debian 12/Bookworm signing keys.  Do not disable signature
  # verification: prefer the host keyring when it already contains all
  # Bookworm keys, otherwise fetch the official Debian public keys over HTTPS,
  # verify their hard-coded fingerprints, and build an OAL-local keyring.
  prepare_bookworm_keyring() {
    local system_keyring=/usr/share/keyrings/debian-archive-keyring.gpg
    local cache_dir="$HOME/.cache/openastrolink/debian-archive-keyring"
    local cached_keyring="$cache_dir/bookworm-archive-keyring.gpg"
    local archive_fpr=B8B80B5B623EAB6AD8775C45B7C5D7D6350947F8
    local release_fpr=4D64FEC119C2029067D6E791F8D2585B8783D481
    local security_fpr=05AB90340C0C5E797F44A8C8254CF3B5AEC0A8F0

    keyring_has_fpr() {
      local ring="$1" expected="$2"
      [[ -s "$ring" ]] || return 1
      gpg --batch --no-default-keyring --keyring "$ring" --with-colons --list-keys 2>/dev/null \
        | awk -F: '$1 == "fpr" { print toupper($10) }' \
        | grep -Fxq "$expected"
    }

    keyring_is_bookworm_complete() {
      local ring="$1"
      keyring_has_fpr "$ring" "$archive_fpr" \
        && keyring_has_fpr "$ring" "$release_fpr" \
        && keyring_has_fpr "$ring" "$security_fpr"
    }

    if keyring_is_bookworm_complete "$system_keyring"; then
      printf '%s\n' "$system_keyring"
      return 0
    fi

    if keyring_is_bookworm_complete "$cached_keyring"; then
      echo "Using cached Debian 12/Bookworm archive keyring: $cached_keyring" >&2
      printf '%s\n' "$cached_keyring"
      return 0
    fi

    echo "Host Debian archive keyring is too old for Bookworm; fetching verified Debian 12 signing keys ..." >&2
    mkdir -p "$cache_dir"
    local tmp
    tmp="$(mktemp -d)"
    local ghome="$tmp/gnupg"
    mkdir -m 700 "$ghome"

    fetch_and_verify_key() {
      # Keep declaration and assignment separate: with `set -u`, Bash expands the
      # complete `local ...` command before `name` has been assigned, so an
      # initializer such as out="$tmp/$name" would raise "name: unbound variable".
      local name expected url out
      name="$1"
      expected="$2"
      url="$3"
      out="$tmp/$name"
      curl --fail --silent --show-error --location --proto '=https' --tlsv1.2 "$url" -o "$out"
      if ! gpg --batch --show-keys --with-colons "$out" 2>/dev/null \
          | awk -F: '$1 == "fpr" { print toupper($10) }' \
          | grep -Fxq "$expected"; then
        echo "ERROR: Debian signing key fingerprint mismatch for $url" >&2
        rm -rf "$tmp"
        return 1
      fi
      gpg --batch --homedir "$ghome" --import "$out" >/dev/null 2>&1
    }

    fetch_and_verify_key archive-key-12.asc "$archive_fpr" \
      https://ftp-master.debian.org/keys/archive-key-12.asc
    fetch_and_verify_key release-12.asc "$release_fpr" \
      https://ftp-master.debian.org/keys/release-12.asc
    fetch_and_verify_key archive-key-12-security.asc "$security_fpr" \
      https://ftp-master.debian.org/keys/archive-key-12-security.asc

    local new_keyring="$tmp/bookworm-archive-keyring.gpg"
    gpg --batch --homedir "$ghome" --export \
      "$archive_fpr" "$release_fpr" "$security_fpr" > "$new_keyring"

    if ! keyring_is_bookworm_complete "$new_keyring"; then
      echo "ERROR: generated Bookworm archive keyring failed fingerprint verification." >&2
      rm -rf "$tmp"
      return 1
    fi

    install -m 0644 "$new_keyring" "$cached_keyring"
    rm -rf "$tmp"
    echo "Prepared verified Debian 12/Bookworm keyring: $cached_keyring" >&2
    printf '%s\n' "$cached_keyring"
  }

  debian_keyring="$(prepare_bookworm_keyring)"

  if [[ ! -x "$sysroot/bin/sh" ]]; then
    echo "Bootstrapping Debian 12/Bookworm $deb_arch sysroot at $sysroot ..."
    sudo mkdir -p "$sysroot"
    sudo debootstrap --arch="$deb_arch" --foreign --keyring="$debian_keyring" \
      bookworm "$sysroot" http://deb.debian.org/debian
  elif [[ ! -f "$marker" ]]; then
    echo "Resuming an incomplete foreign debootstrap at $sysroot ..."
  fi

  sudo mkdir -p "$sysroot/usr/bin"
  sudo cp "$(command -v "$qemu")" "$sysroot/usr/bin/$qemu"

  if [[ ! -f "$marker" ]]; then
    echo "Verifying foreign-architecture execution through binfmt_misc ..."
    if ! sudo chroot "$sysroot" /bin/true; then
      echo "ERROR: ARM binaries still cannot execute inside the sysroot." >&2
      echo "Handler status:" >&2
      cat "/proc/sys/fs/binfmt_misc/$qemu_name" >&2 || true
      exit 9
    fi

    echo "Completing Debian foreign debootstrap second stage ..."
    sudo chroot "$sysroot" /debootstrap/debootstrap --second-stage
    sudo touch "$marker"
  fi

  # Keep the Bookworm sysroot patched and deterministic enough for repeated
  # builds instead of relying on the single debootstrap mirror line only.
  sudo tee "$sysroot/etc/apt/sources.list" >/dev/null <<'APT_SOURCES'
deb http://deb.debian.org/debian bookworm main
deb http://deb.debian.org/debian bookworm-updates main
deb http://security.debian.org/debian-security bookworm-security main
APT_SOURCES

  echo "Installing target Qt/OpenCV/USB development packages in sysroot ..."
  sudo chroot "$sysroot" /usr/bin/apt-get update
  sudo chroot "$sysroot" /usr/bin/env DEBIAN_FRONTEND=noninteractive \
    /usr/bin/apt-get install -y "${target_packages[@]}"
fi

# Convert absolute target links so the host linker never escapes the sysroot.
# debootstrap/apt create much of this tree as root; running symlinks as the
# normal WSL user produced a wall of Permission denied messages and, worse,
# left alternatives-managed BLAS/LAPACK links absolute.  Repair them as root.
echo "Normalizing absolute symlinks inside target sysroot ..."
sudo symlinks -cr "$sysroot" >/dev/null

qt_config="$(find "$sysroot/usr" -type f -path '*/cmake/Qt6/Qt6Config.cmake' -print -quit)"
opencv_config="$(find "$sysroot/usr" -type f -name OpenCVConfig.cmake -print -quit)"
libc="$(find "$sysroot/lib" "$sysroot/usr/lib" -type f -name 'libc.so.6' -print -quit)"
[[ -n "$qt_config" ]] || { echo "ERROR: Qt6Config.cmake missing from sysroot." >&2; exit 6; }
[[ -n "$opencv_config" ]] || { echo "ERROR: OpenCVConfig.cmake missing from sysroot." >&2; exit 7; }

# The final executable link follows OpenCV's transitive numerical stack.
# Validate the two leaf SONAMEs explicitly so a partially mirrored/bootstrapped
# sysroot fails here rather than at 98-100% with hundreds of BLAS/LAPACK
# undefined references.
blas_runtime=""
lapack_runtime=""
for candidate in \
    "$sysroot/usr/lib/$triple/blas/libblas.so.3" \
    "$sysroot/usr/lib/$triple/libblas.so.3"; do
  if [[ -e "$candidate" ]]; then blas_runtime="$candidate"; break; fi
done
for candidate in \
    "$sysroot/usr/lib/$triple/lapack/liblapack.so.3" \
    "$sysroot/usr/lib/$triple/liblapack.so.3"; do
  if [[ -e "$candidate" ]]; then lapack_runtime="$candidate"; break; fi
done
if [[ -z "$blas_runtime" || -z "$lapack_runtime" ]]; then
  echo "ERROR: target BLAS/LAPACK runtime SONAMEs are incomplete after bootstrap." >&2
  echo "Expected libblas.so.3 and liblapack.so.3 under usr/lib/$triple/{blas,lapack}." >&2
  exit 10
fi

# Prove the files that the cross linker will receive are target-architecture
# ELF libraries rather than host fallbacks or dangling alternatives links.
blas_desc="$(file -Lb "$blas_runtime")"
lapack_desc="$(file -Lb "$lapack_runtime")"
if [[ "$arch" == "arm64" ]]; then
  [[ "$blas_desc" == *"ARM aarch64"* ]] || { echo "ERROR: target BLAS is not AArch64: $blas_desc" >&2; exit 10; }
  [[ "$lapack_desc" == *"ARM aarch64"* ]] || { echo "ERROR: target LAPACK is not AArch64: $lapack_desc" >&2; exit 10; }
else
  [[ "$blas_desc" == *"ARM"* && "$blas_desc" != *"aarch64"* ]] || { echo "ERROR: target BLAS is not ARMHF: $blas_desc" >&2; exit 10; }
  [[ "$lapack_desc" == *"ARM"* && "$lapack_desc" != *"aarch64"* ]] || { echo "ERROR: target LAPACK is not ARMHF: $lapack_desc" >&2; exit 10; }
fi

qt_version=""
version_file="$(dirname "$qt_config")/Qt6ConfigVersionImpl.cmake"
if [[ -f "$version_file" ]]; then
  qt_version="$(grep -Eo '[0-9]+\.[0-9]+\.[0-9]+' "$version_file" | head -1 || true)"
fi
qt_version="${qt_version:-6.4.2}"

if [[ -n "$libc" ]]; then
  desc="$(file -b "$libc")"
  echo "Target libc: $desc"
fi

echo "Target Qt config: $qt_config"
echo "Target Qt version: $qt_version"
echo "Target OpenCV: $opencv_config"
echo "Target BLAS: $blas_runtime ($blas_desc)"
echo "Target LAPACK: $lapack_runtime ($lapack_desc)"

qt_host="$HOME/.local/share/openastrolink/host-qt/$qt_version"
if (( bootstrap_host_qt )); then
  "$root/scripts/bootstrap_qt_host.sh" "$qt_version" "$qt_host"
fi

qhy_env=""
if [[ "$qhy_mode" != "off" ]]; then
  if [[ -n "$qhy_version" ]]; then
    echo "Fetching/staging official QHYCCD SDK $qhy_version for $arch ..."
    if "$root/scripts/fetch_qhy_sdk.sh" "$arch" "$qhy_version" "$qhy_dest"; then
      qhy_env="$qhy_dest"
    elif [[ "$qhy_mode" == "required" ]]; then
      echo "ERROR: official QHY SDK $qhy_version could not be staged for $arch." >&2
      exit 8
    else
      echo "WARNING: official QHY SDK $qhy_version could not be staged; continuing with QHY disabled." >&2
    fi
  elif [[ ! -e "$qhy_source" ]]; then
    if [[ "$qhy_mode" == "required" ]]; then
      echo "ERROR: QHY staging is required but the SDK source is missing: $qhy_source" >&2
      exit 8
    fi
    echo "WARNING: QHY SDK source not found; continuing with QHY disabled: $qhy_source" >&2
  else
    echo "Staging QHY SDK for $arch from $qhy_source ..."
    if "$root/scripts/stage_qhy_cross_sdk.sh" "$arch" "$qhy_source" "$qhy_dest"; then
      [[ -f "$qhy_dest/include/qhyccd.h" ]] || { echo "ERROR: QHY staging did not produce qhyccd.h." >&2; exit 8; }
      qhy_found=""
      for candidate in "$qhy_dest/lib/libqhy.so" "$qhy_dest/lib/libqhyccd.so" \
                       "$qhy_dest/lib/libqhyccd.a" "$qhy_dest/lib/libqhy.a"; do
        if [[ -e "$candidate" ]]; then qhy_found="$candidate"; break; fi
      done
      [[ -n "$qhy_found" ]] || { echo "ERROR: QHY staging did not produce a usable SDK library." >&2; exit 8; }
      qhy_env="$qhy_dest"
    elif [[ "$qhy_mode" == "required" ]]; then
      echo "ERROR: a matching QHY SDK is required for this bootstrap." >&2
      exit 8
    else
      echo "WARNING: no QHY SDK matching target '$arch' was staged; continuing with QHY disabled." >&2
      echo "         For ARM64 use a genuine QHY Arm_64/AARCH64 SDK, not the legacy 32-bit 'armv8' archive." >&2
    fi
  fi
fi

mkdir -p "$root/.oal"
cat > "$root/.oal/rpi-cross-${arch}.env" <<ENV
OAS_CROSS_SYSROOT=$sysroot
OAS_QT_HOST_PATH=$qt_host
OAS_QT_TARGET_DIR=$(dirname "$qt_config")
OAS_OPENCV_TARGET_DIR=$(dirname "$opencv_config")
OAS_QHY_SDK=$qhy_env
ENV

qhy_summary="disabled"
if [[ -n "$qhy_env" ]]; then qhy_summary="$qhy_env"; fi

cat <<DONE

Raspberry Pi cross environment is ready.
  arch:          $arch
  sysroot:       $sysroot
  Qt host tools: $qt_host
  QHY SDK:       $qhy_summary
  env record:    $root/.oal/rpi-cross-${arch}.env

Build with your local preset:
  $([[ -n "$qhy_env" ]] && echo "QHY-compatible SDK staged; use the corresponding *-full preset." || echo "QHY is disabled for this target; use the non-full preset.")
  cmake --preset $([[ "$arch" == arm64 ]] && { [[ -n "$qhy_env" ]] && echo my-rpi4-cross-arm64-full || echo my-rpi4-cross-arm64; } || { [[ -n "$qhy_env" ]] && echo my-rpi-cross-armhf-full || echo my-rpi-cross-armhf; }) \
    -DOAS_CROSS_SYSROOT="$sysroot" -DOAS_QT_HOST_PATH="$qt_host" \
    -DQt6_DIR="$(dirname "$qt_config")" -DOpenCV_DIR="$(dirname "$opencv_config")"
DONE
