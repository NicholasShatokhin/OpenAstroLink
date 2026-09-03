#!/usr/bin/env bash
set -euo pipefail

version="${1:-6.4.2}"
prefix="${2:-$HOME/.local/share/openastrolink/host-qt/$version}"
work="${OAL_QT_BOOTSTRAP_WORK:-$HOME/.cache/openastrolink/qt-host-src}"

# Prefer an exact-version Qt Online Installer tree if the user already has one.
installer_qt="$HOME/Qt/$version/gcc_64"
if [[ -f "$installer_qt/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
  echo "Matching Qt Online Installer host Qt found: $installer_qt"
  mkdir -p "$(dirname "$prefix")"
  rm -f "$prefix" 2>/dev/null || true
  ln -sfn "$installer_qt" "$prefix"
  exit 0
fi

if [[ -x "$prefix/libexec/moc" || -x "$prefix/bin/moc" ]]; then
  echo "Host Qt $version already present: $prefix"
  exit 0
fi

for cmd in cmake curl tar g++; do
  if ! command -v "$cmd" >/dev/null 2>&1; then
    echo "ERROR: required host tool '$cmd' is missing." >&2
    echo "On Debian/Ubuntu: sudo apt install build-essential cmake curl xz-utils" >&2
    exit 2
  fi
done

minor="${version%.*}"
archive="qtbase-everywhere-src-${version}.tar.xz"
url="https://download.qt.io/archive/qt/${minor}/${version}/submodules/${archive}"
mkdir -p "$work" "$prefix"

if [[ ! -f "$work/$archive" ]]; then
  echo "Downloading Qt host tools source: $url"
  curl --fail --location --retry 3 -o "$work/$archive" "$url"
fi

src="$work/qtbase-everywhere-src-${version}"
build="$work/build-${version}"
if [[ ! -d "$src" ]]; then
  tar -xf "$work/$archive" -C "$work"
fi
rm -rf "$build"

# Cross Qt only needs runnable host tools (not a second desktop Qt installation).
# Keep this build intentionally small and independent from the host distro Qt.
cmake -S "$src" -B "$build" -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$prefix" \
  -DQT_BUILD_EXAMPLES=OFF \
  -DQT_BUILD_TESTS=OFF \
  -DFEATURE_gui=OFF \
  -DFEATURE_widgets=OFF \
  -DFEATURE_opengl=OFF \
  -DFEATURE_printsupport=OFF \
  -DFEATURE_sql=OFF \
  -DFEATURE_dbus=OFF
cmake --build "$build" -j"$(nproc 2>/dev/null || echo 4)"
cmake --install "$build"

moc="$(find "$prefix" -type f -name moc -perm -u+x -print -quit)"
if [[ -z "$moc" ]]; then
  echo "ERROR: Qt host bootstrap completed but moc was not installed under $prefix" >&2
  exit 3
fi

echo "Host Qt tools ready: $prefix"
echo "  moc: $moc"
