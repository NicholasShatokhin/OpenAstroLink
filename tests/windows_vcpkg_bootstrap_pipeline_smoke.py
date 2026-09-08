#!/usr/bin/env python3
"""Guard the Windows vcpkg bootstrap against PowerShell pipeline contamination."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
p = root / "scripts" / "bootstrap_native_dependencies.ps1"
text = p.read_text(encoding="utf-8-sig")

required = [
    "| Out-Host",
    "return [string]$vcpkgRoot",
    "Join-Path $vcpkgRoot 'vcpkg.exe'",
    "vcpkg bootstrap completed but executable was not created",
    "& $vcpkgExe install libdatachannel:x64-windows --disable-metrics",
]
for token in required:
    assert token in text, token

# The original bug invoked an interpolated path returned from a noisy function.
assert '& "$vcpkg\\vcpkg.exe"' not in text
print("windows vcpkg bootstrap pipeline smoke: PASS")
