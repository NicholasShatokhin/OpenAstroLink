#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
qt_version="${OAL_NATIVE_QT_VERSION:-6.8.3}"
dest_root="${OAL_QT_INSTALL_ROOT:-$HOME/.local/share/openastrolink/qt}"
force=0

usage() {
  cat <<USAGE
Usage: $0 [--version X.Y.Z] [--dest-root DIR] [--force]

Finds a usable host-native Qt >= 6.4 with the OpenAstroLink modules. If none is
available, downloads Qt binaries through aqtinstall into a per-user OAL cache.
No Qt files are installed into /usr or /usr/local.

Required modules: Core, Gui, Network, Widgets, Concurrent, SerialPort,
WebSockets, HttpServer, Positioning.
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --version) qt_version="$2"; shift 2 ;;
    --dest-root) dest_root="$2"; shift 2 ;;
    --force) force=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
  esac
done

os="$(uname -s)"
arch="$(uname -m)"
case "$os" in
  Linux)
    platform="linux"
    case "$arch" in
      x86_64|amd64) norm_arch="x86_64"; aqt_os="linux"; aqt_arch="linux_gcc_64" ;;
      aarch64|arm64) norm_arch="arm64"; aqt_os="linux_arm64"; aqt_arch="linux_gcc_arm64" ;;
      *) echo "ERROR: unsupported Linux architecture for native Qt bootstrap: $arch" >&2; exit 3 ;;
    esac
    ;;
  Darwin)
    platform="macos"
    case "$arch" in
      x86_64|amd64) norm_arch="x86_64" ;;
      aarch64|arm64) norm_arch="arm64" ;;
      *) echo "ERROR: unsupported macOS architecture for native Qt bootstrap: $arch" >&2; exit 3 ;;
    esac
    aqt_os="mac"
    aqt_arch="clang_64"
    ;;
  *) echo "ERROR: use bootstrap_qt_native.ps1 on Windows." >&2; exit 3 ;;
esac

version_ge() {
  local have="$1" need="$2"
  awk -v have="$have" -v need="$need" 'BEGIN {
    split(have,h,"."); split(need,n,".");
    for (i=1;i<=4;i++) { hv=(h[i]==""?0:h[i])+0; nv=(n[i]==""?0:n[i])+0; if (hv>nv) exit 0; if (hv<nv) exit 1; }
    exit 0
  }'
}

qt_version_from_prefix() {
  local p="$1"
  local cfg="$p/lib/cmake/Qt6/Qt6ConfigVersion.cmake"
  if [[ -x "$p/bin/qtpaths6" ]]; then "$p/bin/qtpaths6" --qt-version 2>/dev/null && return 0; fi
  if [[ -x "$p/bin/qtpaths" ]]; then "$p/bin/qtpaths" --qt-version 2>/dev/null && return 0; fi
  if [[ -f "$cfg" ]]; then
    sed -n 's/^[[:space:]]*set(PACKAGE_VERSION[[:space:]]*"\([^"]*\)".*/\1/p' "$cfg" | head -n1
    return 0
  fi
  return 1
}

qt_prefix_ok() {
  local p="$1" v=""
  [[ -n "$p" && -f "$p/lib/cmake/Qt6/Qt6Config.cmake" ]] || return 1
  for mod in Core Gui Network Widgets Concurrent SerialPort WebSockets HttpServer Positioning; do
    [[ -f "$p/lib/cmake/Qt6${mod}/Qt6${mod}Config.cmake" ]] || return 1
  done
  v="$(qt_version_from_prefix "$p" || true)"
  [[ -n "$v" ]] || return 1
  version_ge "$v" 6.4.0 || return 1
  printf '%s\n' "$v"
}

candidates=()
[[ -n "${OAS_QT_ROOT:-}" ]] && candidates+=("$OAS_QT_ROOT")
[[ -n "${QTDIR:-}" ]] && candidates+=("$QTDIR")

# OAL-managed installations and the standard Qt Online Installer layout.
if [[ "$platform" == linux ]]; then
  candidates+=("$dest_root/$qt_version/gcc_64" "$HOME/Qt/$qt_version/gcc_64")
  while IFS= read -r cfg; do candidates+=("${cfg%/lib/cmake/Qt6/Qt6Config.cmake}"); done < <(find "$dest_root" "$HOME/Qt" -type f -path '*/gcc_64/lib/cmake/Qt6/Qt6Config.cmake' -print 2>/dev/null | sort -r)
else
  # aqt's macOS desktop package normally installs under <version>/macos.
  candidates+=("$dest_root/$qt_version/macos" "$HOME/Qt/$qt_version/macos" "/opt/homebrew/opt/qt" "/usr/local/opt/qt")
  while IFS= read -r cfg; do candidates+=("${cfg%/lib/cmake/Qt6/Qt6Config.cmake}"); done < <(find "$dest_root" "$HOME/Qt" -type f -path '*/lib/cmake/Qt6/Qt6Config.cmake' -print 2>/dev/null | sort -r)
fi

# Also consider the active system Qt if it is complete and new enough.
if command -v qtpaths6 >/dev/null 2>&1; then
  p="$(qtpaths6 --query QT_INSTALL_PREFIX 2>/dev/null || true)"
  [[ -n "$p" ]] && candidates+=("$p")
fi

if [[ $force -eq 0 ]]; then
  seen=""
  for p in "${candidates[@]}"; do
    [[ -n "$p" ]] || continue
    case ":$seen:" in *":$p:"*) continue ;; esac
    seen="$seen:$p"
    if v="$(qt_prefix_ok "$p" 2>/dev/null)"; then
      echo "Using native Qt $v: $p"
      mkdir -p "$root/.oal"
      env_file="$root/.oal/native-qt-${platform}-${norm_arch}.env"
      printf 'OAS_QT_ROOT=%q\nCMAKE_PREFIX_PATH=%q\nOAS_QT_VERSION=%q\n' "$p" "$p" "$v" > "$env_file"
      echo "Qt environment record: $env_file"
      exit 0
    fi
  done
fi

command -v python3 >/dev/null 2>&1 || {
  echo "ERROR: Python 3 is required to bootstrap Qt with aqtinstall." >&2
  echo "Run scripts/bootstrap_native_dependencies.sh so the platform prerequisites can be installed first." >&2
  exit 4
}

venv="${OAL_AQT_VENV:-$HOME/.cache/openastrolink/aqt-venv}"
if [[ ! -x "$venv/bin/python" ]]; then
  echo "Creating aqtinstall environment: $venv"
  python3 -m venv "$venv" || {
    echo "ERROR: python3 -m venv failed. On Debian/Ubuntu install python3-venv." >&2
    exit 5
  }
fi
"$venv/bin/python" -m pip -q install --upgrade pip
"$venv/bin/python" -m pip -q install --upgrade 'aqtinstall>=3.2,<4'

echo "Resolving Qt $qt_version packages for $platform/$norm_arch through aqtinstall ..."

# aqt architecture identifiers are repository identifiers, not install-directory
# names.  In particular, Linux x86_64 uses `linux_gcc_64` even though Qt is
# installed into a directory conventionally named `gcc_64`.  Passing `gcc_64`
# makes recent aqt versions fail with a misleading "qt_base package not found"
# error.
available_arches="$("$venv/bin/aqt" list-qt "$aqt_os" desktop --arch "$qt_version" 2>/dev/null || true)"
if ! printf '%s\n' "$available_arches" | tr ' ' '\n' | grep -Fxq "$aqt_arch"; then
  echo "ERROR: Qt $qt_version does not advertise architecture '$aqt_arch' for $aqt_os/desktop." >&2
  echo "Available architectures: ${available_arches:-<none>}" >&2
  exit 6
fi

available_modules="$("$venv/bin/aqt" list-qt "$aqt_os" desktop --modules "$qt_version" "$aqt_arch" 2>/dev/null || true)"
required_modules=(qthttpserver qtwebsockets qtserialport qtpositioning)
missing_modules=()
for mod in "${required_modules[@]}"; do
  if ! printf '%s\n' "$available_modules" | tr ' ' '\n' | grep -Fxq "$mod"; then
    missing_modules+=("$mod")
  fi
done
if [[ ${#missing_modules[@]} -gt 0 ]]; then
  echo "ERROR: Qt $qt_version/$aqt_arch is missing required modules: ${missing_modules[*]}" >&2
  echo "aqt reported modules: ${available_modules:-<none>}" >&2
  exit 7
fi

echo "Downloading Qt $qt_version for $platform/$norm_arch through aqtinstall ..."
mkdir -p "$dest_root"
"$venv/bin/aqt" install-qt "$aqt_os" desktop "$qt_version" "$aqt_arch" \
  --outputdir "$dest_root" \
  --modules "${required_modules[@]}"

# Do not assume aqt's final directory spelling; locate and validate the result.
installed=""
while IFS= read -r cfg; do
  p="${cfg%/lib/cmake/Qt6/Qt6Config.cmake}"
  if v="$(qt_prefix_ok "$p" 2>/dev/null)"; then
    installed="$p"
    break
  fi
done < <(find "$dest_root/$qt_version" -type f -path '*/lib/cmake/Qt6/Qt6Config.cmake' 2>/dev/null | sort)

[[ -n "$installed" ]] || {
  echo "ERROR: aqtinstall completed, but a complete Qt >= 6.4 installation was not found under $dest_root/$qt_version." >&2
  exit 8
}

v="$(qt_version_from_prefix "$installed")"
mkdir -p "$root/.oal"
env_file="$root/.oal/native-qt-${platform}-${norm_arch}.env"
printf 'OAS_QT_ROOT=%q\nCMAKE_PREFIX_PATH=%q\nOAS_QT_VERSION=%q\n' "$installed" "$installed" "$v" > "$env_file"

echo "Native Qt is ready:"
echo "  version:    $v"
echo "  prefix:     $installed"
echo "  env record: $env_file"
