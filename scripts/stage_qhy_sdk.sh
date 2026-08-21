#!/usr/bin/env bash
set -euo pipefail
if [[ $EUID -ne 0 ]]; then echo "Run with sudo: sudo $0 [/tmp/oal-qhy-sdk]" >&2; exit 1; fi
STAGE="${1:-/tmp/oal-qhy-sdk}"
HDR="$STAGE/QHYCCD_Linux/qhyccd.h"
LIB="$(find "$STAGE/lib" -maxdepth 1 -type f -name 'libqhy.so*' | head -1 || true)"
[[ -f "$HDR" ]] || { echo "Missing $HDR" >&2; exit 2; }
[[ -n "$LIB" && -f "$LIB" ]] || { echo "No libqhy.so* under $STAGE/lib" >&2; exit 3; }
ARCH="$(uname -m)"
echo "Pi architecture: $ARCH"
file "$LIB"
if [[ "$ARCH" == "aarch64" ]]; then
  file "$LIB" | grep -Eqi 'aarch64|ARM aarch64' || { echo "QHY library is not AArch64; refusing to install" >&2; exit 4; }
fi
install -d -m 0755 /opt/qhyccd/include /opt/qhyccd/lib /opt/qhyccd/vendor
install -m 0644 "$STAGE"/QHYCCD_Linux/*.h /opt/qhyccd/include/
cp -a "$STAGE/QHYCCD_Linux/." /opt/qhyccd/vendor/
install -m 0755 "$LIB" "/opt/qhyccd/lib/$(basename "$LIB")"
ln -sfn "$(basename "$LIB")" /opt/qhyccd/lib/libqhy.so
ldconfig
echo "QHY SDK staged. Configure OAL with:"
echo "  -DQHYCCD_INCLUDE_DIR=/opt/qhyccd/include -DQHYCCD_LIBRARY=/opt/qhyccd/lib/libqhy.so"
echo "Vendor firmware/assets retained under /opt/qhyccd/vendor; install vendor udev rules separately if the package provides them."
