#!/usr/bin/env bash
set -euo pipefail
if [[ $EUID -ne 0 ]]; then echo "Run with sudo: sudo $0 [build-dir]" >&2; exit 1; fi
BUILD_DIR="${1:-build-rpi}"
OAL_USER="${SUDO_USER:-pi}"
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_ROOT="$ROOT_DIR/$BUILD_DIR"

install -m 0755 "$BUILD_ROOT/openastrolink-node" /usr/local/bin/openastrolink-node
if [[ -x "$BUILD_ROOT/oal-hardware-probe" ]]; then
  install -m 0755 "$BUILD_ROOT/oal-hardware-probe" /usr/local/bin/oal-hardware-probe
fi
if [[ -x "$BUILD_ROOT/OpenAstroSuite" ]]; then
  install -m 0755 "$BUILD_ROOT/OpenAstroSuite" /usr/local/bin/OpenAstroSuite
  install -m 0644 "$ROOT_DIR/packaging/linux/openastrosuite-local.desktop" /usr/local/share/applications/openastrosuite-local.desktop
fi

# Native OAL ABI-v2 drivers are installed independently of the node binary.
# The node scans this location before compatibility backends are considered.
install -d -m 0755 /usr/local/lib/openastrolink/drivers
if [[ -d "$BUILD_ROOT/drivers" ]]; then
  shopt -s nullglob
  for f in "$BUILD_ROOT"/drivers/*.so "$BUILD_ROOT"/drivers/*.manifest.json; do
    install -m 0755 "$f" /usr/local/lib/openastrolink/drivers/$(basename "$f")
    [[ "$f" == *.manifest.json ]] && chmod 0644 /usr/local/lib/openastrolink/drivers/$(basename "$f")
  done
  shopt -u nullglob
fi

install -d -m 0755 /etc/openastrolink
install -d -o "$OAL_USER" -g "$OAL_USER" -m 0755 /var/lib/openastrolink /var/lib/openastrolink/captures /var/lib/openastrolink/captures/canon
if [[ ! -e /etc/openastrolink/node.env ]]; then
  install -m 0644 "$ROOT_DIR/packaging/systemd/openastrolink-node.env.example" /etc/openastrolink/node.env
fi
sed "s/@OAL_USER@/$OAL_USER/g" "$ROOT_DIR/packaging/systemd/openastrolink-node.service.in" > /etc/systemd/system/openastrolink-node.service
systemctl daemon-reload
systemctl enable --now openastrolink-node.service

echo "Installed OpenAstroLink node for user $OAL_USER."
echo "Native drivers: /usr/local/lib/openastrolink/drivers"
echo "Check: systemctl status openastrolink-node"
echo "Logs:  journalctl -u openastrolink-node -f"
echo "HTTP: http://$(hostname -I | awk '{print $1}'):8080/api/v1/node/info"
echo "Drivers: http://$(hostname -I | awk '{print $1}'):8080/api/v1/drivers"
if command -v oal-hardware-probe >/dev/null 2>&1; then
  echo "Hardware probe: oal-hardware-probe"
fi
if [[ -x /usr/local/bin/OpenAstroSuite ]]; then
  echo "Local GUI: OpenAstroSuite --node http://127.0.0.1:8080"
fi
