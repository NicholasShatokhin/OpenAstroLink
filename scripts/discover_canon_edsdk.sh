#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
os="$(uname -s)"
arch="$(uname -m)"
case "$os" in
  Linux) platform="linux" ;;
  Darwin) platform="macos" ;;
  *) echo "ERROR: discover_canon_edsdk.sh supports Linux/macOS." >&2; exit 2 ;;
esac
case "$arch" in
  x86_64|amd64) norm_arch="x86_64" ;;
  aarch64|arm64) norm_arch="arm64" ;;
  *) norm_arch="$arch" ;;
esac

search_roots=()
[[ -n "${CANON_EDSDK_ROOT:-}" ]] && search_roots+=("$CANON_EDSDK_ROOT")
[[ -n "${OAS_CANON_EDSDK_ROOT:-}" ]] && search_roots+=("$OAS_CANON_EDSDK_ROOT")
search_roots+=(
  "$root/../edsdk"
  "$HOME/SDK/Canon"
  "$HOME/SDK/EDSDK"
  "/opt/openastrolink-sdk/canon"
  "/opt/Canon/EDSDK"
)
# WSL convenience; harmless elsewhere.
[[ -d /mnt/c/workspace/astro/edsdk ]] && search_roots+=("/mnt/c/workspace/astro/edsdk")

header=""
lib=""
for r in "${search_roots[@]}"; do
  [[ -d "$r" ]] || continue
  h="$(find "$r" -type f -name EDSDK.h -print -quit 2>/dev/null || true)"
  [[ -n "$h" ]] || continue
  if [[ "$platform" == linux ]]; then
    while IFS= read -r candidate; do
      d="$(file -L "$candidate" 2>/dev/null || true)"
      if [[ "$norm_arch" == arm64 ]]; then
        grep -Eqi 'ELF 64-bit.*ARM aarch64' <<<"$d" || continue
      else
        grep -Eqi 'ELF 64-bit.*x86-64' <<<"$d" || continue
      fi
      header="$h"; lib="$candidate"; break
    done < <(find "$r" -type f -name 'libEDSDK.so*' -print 2>/dev/null)
  else
    while IFS= read -r candidate; do
      d="$(file -L "$candidate" 2>/dev/null || true)"
      if [[ "$norm_arch" == arm64 ]]; then
        grep -Eqi 'Mach-O.*arm64|arm64' <<<"$d" || continue
      else
        grep -Eqi 'Mach-O.*x86_64|x86_64' <<<"$d" || continue
      fi
      header="$h"; lib="$candidate"; break
    done < <(find "$r" -type f \( -name 'libEDSDK*.dylib' -o -path '*/EDSDK.framework/EDSDK' \) -print 2>/dev/null)
  fi
  [[ -n "$lib" ]] && break
done

if [[ -z "$header" || -z "$lib" ]]; then
  echo "Canon EDSDK was not auto-discovered for $platform/$norm_arch."
  echo "This SDK is license-gated by Canon, so OpenAstroLink does not download it automatically."
  exit 1
fi

mkdir -p "$root/.oal"
env_file="$root/.oal/native-canon-${platform}-${norm_arch}.env"
printf 'CANON_EDSDK_INCLUDE_DIR=%q\nCANON_EDSDK_LIBRARY=%q\nCANON_EDSDK_ROOT=%q\n' \
  "$(dirname "$header")" "$lib" "$(dirname "$(dirname "$header")")" > "$env_file"
if [[ "$platform" == macos ]]; then
  printf 'CANON_EDSDK_RUNTIME_DIR=%q\n' "$(dirname "$lib")" >> "$env_file"
fi

echo "Canon EDSDK discovered:"
echo "  include:    $(dirname "$header")"
echo "  library:    $lib"
echo "  env record: $env_file"
