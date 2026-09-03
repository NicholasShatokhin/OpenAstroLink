#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
arch="${1:-arm64}"
source_path="${2:-$root/../QHYCCD_Linux_New}"
dest="${3:-}"

case "$arch" in
  arm64|aarch64)
    arch="arm64"
    dest="${dest:-$HOME/.local/share/openastrolink/sdk/qhy-arm64}"
    file_re='ARM aarch64|aarch64'
    # QHY has historically used several ambiguous names.  In particular the
    # legacy QHYCCD_Linux_New *armv8* archive is actually a 32-bit ARM EABI
    # build, so filenames are only hints; the ELF architecture is authoritative.
    candidate_name_re='(aarch64|arm64|armv8)'
    ;;
  armhf|arm32)
    arch="armhf"
    dest="${dest:-$HOME/.local/share/openastrolink/sdk/qhy-armhf}"
    file_re='ELF 32-bit.*ARM'
    # Include armv8 here deliberately: the legacy public QHYCCD_Linux_New
    # armv8 package contains 32-bit ARM/EABI libraries despite its name.
    candidate_name_re='(arm32|armhf|armv7|rpi|armv8)'
    ;;
  x86_64|amd64)
    arch="x86_64"
    dest="${dest:-$HOME/.local/share/openastrolink/sdk/qhy-x86_64}"
    file_re='ELF 64-bit.*x86-64|x86-64'
    candidate_name_re='(linux64|x86_64|x64)'
    ;;
  *)
    echo "Usage: $0 [arm64|armhf|x86_64] [QHY SDK checkout/archive] [DEST]" >&2
    exit 2
    ;;
esac

if [[ ! -e "$source_path" ]]; then
  echo "ERROR: QHY SDK source not found: $source_path" >&2
  echo "Pass either a QHY SDK archive (.tgz/.tar.gz) or a directory containing SDK archives." >&2
  exit 3
fi

archives=()
if [[ -f "$source_path" ]]; then
  archives+=("$source_path")
elif [[ -d "$source_path" ]]; then
  # First collect architecture-looking archives.  Do not trust the name for
  # final selection; every extracted library is checked with file(1).
  while IFS= read -r p; do archives+=("$p"); done < <(
    find "$source_path" -maxdepth 2 -type f \( -name '*.tar.gz' -o -name '*.tgz' \) -print \
      | grep -Ei "$candidate_name_re" | sort -Vr || true
  )
fi

if (( ${#archives[@]} == 0 )); then
  echo "ERROR: no plausible QHY SDK archive for target '$arch' was found under $source_path" >&2
  exit 4
fi

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

selected=""
selected_header=""
selected_archive=""
all_diagnostics=()

for archive in "${archives[@]}"; do
  extract_dir="$tmp/extract-$(printf '%s' "$archive" | sha256sum | cut -c1-12)"
  mkdir -p "$extract_dir"
  echo "QHY candidate archive: $archive"
  if ! tar -xzf "$archive" -C "$extract_dir"; then
    all_diagnostics+=("$archive: extraction failed")
    continue
  fi

  header="$(find -L "$extract_dir" -type f -name qhyccd.h -print -quit 2>/dev/null || true)"
  if [[ -z "$header" ]]; then
    all_diagnostics+=("$archive: qhyccd.h not found")
    continue
  fi

  # Prefer a shared SDK when QHY ships one: OAL loads each vendor driver as
  # its own shared module, so this avoids unnecessary static symbol collisions.
  # Newer official SDKs also ship libqhyccd.a, which is accepted as a fallback.
  mapfile -t shared_libs < <(find -L "$extract_dir" -type f \
    \( -name 'libqhy.so' -o -name 'libqhy.so.*' -o -name 'libqhyccd.so' -o -name 'libqhyccd.so.*' \) \
    -size +0c -print 2>/dev/null | sort -V)
  mapfile -t static_libs < <(find -L "$extract_dir" -type f \
    \( -name 'libqhy.a' -o -name 'libqhyccd.a' \) \
    -size +0c -print 2>/dev/null | sort -V)

  libs=("${shared_libs[@]}" "${static_libs[@]}")
  if (( ${#libs[@]} == 0 )); then
    all_diagnostics+=("$archive: no non-empty libqhy/libqhyccd shared or static library")
    continue
  fi

  for lib in "${libs[@]}"; do
    if [[ "$lib" == *.a ]]; then
      ar_tmp="$extract_dir/.oal-ar-check"
      rm -rf "$ar_tmp" && mkdir -p "$ar_tmp"
      member="$(ar t "$lib" 2>/dev/null | grep -E '\.o$' | head -1 || true)"
      if [[ -z "$member" ]]; then
        desc="static archive with no inspectable .o member"
      else
        (cd "$ar_tmp" && ar x "$lib" "$member")
        member_path="$ar_tmp/$member"
        desc="$(file -Lb "$member_path" 2>/dev/null || true)"
      fi
    else
      desc="$(file -Lb "$lib")"
    fi
    all_diagnostics+=("$archive :: $lib :: $desc")
    if grep -Eiq "$file_re" <<<"$desc"; then
      selected="$lib"
      selected_header="$header"
      selected_archive="$archive"
      break 2
    fi
  done
done

if [[ -z "$selected" ]]; then
  echo "ERROR: no QHY library matching target architecture '$arch' was found." >&2
  echo "The archive filename is not treated as proof of architecture." >&2
  echo "Inspected candidates:" >&2
  for line in "${all_diagnostics[@]}"; do printf '  %s\n' "$line" >&2; done
  if [[ "$arch" == arm64 ]]; then
    cat >&2 <<'MSG'
NOTE: the legacy qhyccd-lzr/QHYCCD_Linux_New file named
      qhyccdsdk-v2.0.11-Linux-Debian-Ubuntu-armv8.tar.gz
      contains a 32-bit ARM EABI library, not AArch64.  Use a current QHY
      Arm_64/AARCH64 SDK archive for a 64-bit Raspberry Pi build, or build the
      QHY-enabled OAL node as ARMHF instead.
MSG
  fi
  exit 7
fi

include_dir="$(dirname "$selected_header")"
rm -rf "$dest"
mkdir -p "$dest/include" "$dest/lib" "$dest/vendor"
cp -aL "$include_dir"/. "$dest/include/"

selected_base="$(basename "$selected")"
cp -aL "$selected" "$dest/lib/$selected_base"
if [[ "$selected_base" == *.a ]]; then
  staged_library="$dest/lib/$selected_base"
else
  if [[ "$selected_base" != "libqhy.so" ]]; then
    ln -sfn "$selected_base" "$dest/lib/libqhy.so"
  fi
  staged_library="$dest/lib/libqhy.so"
fi

# Keep deployment resources when available, without installing target files on
# the x86_64 build host.
selected_root="$(dirname "$selected_header")"
for item in udev firmware fx3load install_scripts; do
  found="$(find "$(dirname "$selected_root")" -type d -name "$item" -print -quit 2>/dev/null || true)"
  if [[ -n "$found" ]]; then cp -a "$found" "$dest/vendor/$item"; fi
done

if [[ ! -f "$dest/include/qhyccd.h" ]]; then
  echo "ERROR: staged QHY SDK is missing $dest/include/qhyccd.h" >&2
  exit 8
fi
if [[ ! -e "$staged_library" ]]; then
  echo "ERROR: staged QHY SDK library is missing: $staged_library" >&2
  exit 9
fi
if [[ "$staged_library" == *.a ]]; then
  verify_tmp="$tmp/staged-ar-check"
  mkdir -p "$verify_tmp"
  verify_member="$(ar t "$staged_library" | grep -E '\.o$' | head -1 || true)"
  [[ -n "$verify_member" ]] || { echo "ERROR: staged QHY static archive has no inspectable object member." >&2; exit 10; }
  (cd "$verify_tmp" && ar x "$staged_library" "$verify_member")
  staged_desc="$(file -Lb "$verify_tmp/$verify_member")"
else
  staged_desc="$(file -Lb "$staged_library")"
fi
if ! grep -Eiq "$file_re" <<<"$staged_desc"; then
  echo "ERROR: staged QHY library has the wrong architecture: $staged_desc" >&2
  exit 10
fi

cat > "$dest/OAL_QHY_SDK.txt" <<META
source_archive=$selected_archive
target_arch=$arch
header_source=$selected_header
include_staged=$dest/include/qhyccd.h
library_source=$selected
library_staged=$staged_library
library_file=$staged_desc
META

echo "QHY source archive: $selected_archive"
echo "QHY SDK staged: $dest"
echo "  include: $dest/include/qhyccd.h"
echo "  library: $staged_library"
echo "  arch:    $staged_desc"
