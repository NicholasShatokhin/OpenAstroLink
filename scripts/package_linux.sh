#!/usr/bin/env bash
set -euo pipefail
build_dir="${1:?Usage: package_linux.sh BUILD_DIR [OUTPUT_DIR]}"
out_dir="${2:-dist/linux}"
root="$(cd "$(dirname "$0")/.." && pwd)"
rm -rf "$root/$out_dir"
mkdir -p "$root/$out_dir"
cmake --install "$build_dir" --prefix "$root/$out_dir/usr/local"
if [[ -f "$build_dir/build_features.json" ]]; then cp "$build_dir/build_features.json" "$root/$out_dir/"; fi
cat > "$root/$out_dir/RUNTIME_DEPENDENCIES.txt" <<'TXT'
Vendor SDK shared libraries are intentionally not copied automatically because redistribution terms differ by vendor.
Install the matching architecture QHYCCD/ZWO/Canon transport runtime on the target host, or bundle it only when its license permits redistribution.
Use `ldd` on bin/openastrolink-node and lib/openastrolink/drivers/*.so to verify runtime resolution.
TXT
(
  cd "$root/$out_dir"
  tar -czf "../$(basename "$out_dir").tar.gz" .
)
echo "Linux package: $root/$out_dir"
