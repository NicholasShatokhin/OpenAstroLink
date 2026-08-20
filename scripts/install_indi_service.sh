#!/usr/bin/env bash
set -euo pipefail
if [[ $EUID -ne 0 ]]; then echo "Run with sudo: sudo $0 driver1 [driver2 ...]" >&2; exit 1; fi
if [[ $# -lt 1 ]]; then
  echo "Usage: sudo $0 <indi_driver_executable> [more drivers...]" >&2
  echo "Installed candidates:" >&2
  find /usr/bin -maxdepth 1 -type f -name 'indi_*' -printf '%f\n' 2>/dev/null | sort | head -200 >&2
  exit 2
fi

for d in "$@"; do
  if ! command -v "$d" >/dev/null 2>&1; then
    echo "INDI driver not found in PATH: $d" >&2
    exit 3
  fi
done

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
install -d -m 0755 /etc/openastrolink
printf 'INDI_DRIVERS="' > /etc/openastrolink/indi-drivers.conf
printf '%s ' "$@" | sed 's/ $//' >> /etc/openastrolink/indi-drivers.conf
printf '"\n' >> /etc/openastrolink/indi-drivers.conf
chmod 0644 /etc/openastrolink/indi-drivers.conf
install -m 0644 "$ROOT_DIR/packaging/systemd/openastrolink-indi.service" /etc/systemd/system/openastrolink-indi.service
systemctl daemon-reload
systemctl enable --now openastrolink-indi.service

echo "INDI service installed: $*"
echo "Check: systemctl status openastrolink-indi"
echo "Logs:  journalctl -u openastrolink-indi -f"
echo "List devices after startup: indi_getprop -h 127.0.0.1 -p 7624 '*.CONNECTION.*'"
