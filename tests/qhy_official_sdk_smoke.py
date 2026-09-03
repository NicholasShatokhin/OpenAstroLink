#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
fetch = (root / "scripts" / "fetch_qhy_sdk.sh").read_text()
stage = (root / "scripts" / "stage_qhy_cross_sdk.sh").read_text()
build = (root / "scripts" / "build_rpi_cross.sh").read_text()
bootstrap = (root / "scripts" / "bootstrap_rpi_cross.sh").read_text()
cmake = (root / "CMakeLists.txt").read_text()

for token in (
    "sdk_linux_arm64_${version}.tar.gz",
    "https://www.qhyccd.com/file/repository/publish/SDK/",
    "260604",
    "curl --fail --location",
    "stage_qhy_cross_sdk.sh",
):
    assert token in fetch, token

for token in ("libqhyccd.a", "ar t", "shared or static library", "staged_library"):
    assert token in stage, token

for token in ("--qhy-version", "fetch_qhy_sdk.sh", 'qhy_version=""'):
    assert token in bootstrap, token

for token in ("libqhyccd.a", "QHY target library"):
    assert token in build, token

assert 'QHYCCD_LIBRARY MATCHES "\\\\.a$"' in cmake
assert "usb-1.0" in cmake
print("QHY official SDK smoke: PASS")
