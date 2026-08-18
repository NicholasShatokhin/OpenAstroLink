#!/usr/bin/env bash
set -euo pipefail
if [[ $EUID -ne 0 ]]; then echo "Run with sudo: sudo $0 [build-dir]" >&2; exit 1; fi
BUILD_DIR="${1:-build-rpi}"
OAL_USER="${SUDO_USER:-pi}"
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

install -m 0755 "$ROOT_DIR/$BUILD_DIR/openastrolink-node" /usr/local/bin/openastrolink-node
if [[ -x "$ROOT_DIR/$BUILD_DIR/OpenAstroSuite" ]]; then
  install -m 0755 "$ROOT_DIR/$BUILD_DIR/OpenAstroSuite" /usr/local/bin/OpenAstroSuite
  install -m 0644 "$ROOT_DIR/packaging/linux/openastrosuite-local.desktop" /usr/local/share/applications/openastrosuite-local.desktop
fi

sed "s/@OAL_USER@/$OAL_USER/g" "$ROOT_DIR/packaging/systemd/openastrolink-node.service.in" > /etc/systemd/system/openastrolink-node.service
systemctl daemon-reload
systemctl enable --now openastrolink-node.service

echo "Installed OpenAstroLink node for user $OAL_USER."
echo "Check: systemctl status openastrolink-node"
echo "Logs:  journalctl -u openastrolink-node -f"
echo "HTTP: http://$(hostname -I | awk '{print $1}'):8080/api/v1/node/info"
if [[ -x /usr/local/bin/OpenAstroSuite ]]; then
  echo "Local GUI: OpenAstroSuite --node http://127.0.0.1:8080"
fi
