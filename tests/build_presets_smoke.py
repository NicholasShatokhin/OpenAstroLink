#!/usr/bin/env python3
import json
from pathlib import Path
root = Path(__file__).resolve().parents[1]
d = json.loads((root / "CMakePresets.json").read_text())
cp = {p["name"]: p for p in d["configurePresets"]}
required = {
    "linux-native-release",
    "rpi4-node-release",
    "rpi-armhf-node-release",
    "rpi4-cross-arm64-node-release",
    "rpi-cross-armhf-node-release",
    "rpi4-cross-arm64-node-windows-release",
    "rpi-cross-armhf-node-windows-release",
    "macos-core-release",
    "macos-native-release",
    "macos-observatory-release",
    "macos-node-release",
    "macos-arm64-observatory-release",
    "macos-x86_64-observatory-release",
    "windows-observatory-indi-release",
    "linux-observatory-indi-release",
    "linux-node-indi-release",
    "rpi4-observatory-indi-release",
    "rpi4-node-indi-release",
    "macos-observatory-indi-release",
    "macos-node-indi-release",
}
missing = sorted(required - cp.keys())
assert not missing, f"missing presets: {missing}"
assert cp["linux-native-release"]["inherits"] == "unix-makefiles-release"
assert cp["macos-native-release"]["inherits"] == "unix-makefiles-release"
assert cp["macos-native-release"]["cacheVariables"]["OAS_CANON_TRANSPORT"] == "EDSDK"
assert cp["macos-native-release"]["cacheVariables"]["OAS_ENABLE_ASCOM_CLASSIC"] == "OFF"
for name in ("rpi4-cross-arm64-node-release", "rpi-cross-armhf-node-release",
             "rpi4-cross-arm64-node-windows-release", "rpi-cross-armhf-node-windows-release"):
    cv = cp[name]["cacheVariables"]
    assert cv["OAS_BUILD_GUI"] == "OFF"
    for key in ("OAS_ENABLE_QHY", "OAS_ENABLE_NATIVE_CANON", "OAS_ENABLE_NATIVE_ZWO_ASI", "OAS_ENABLE_NATIVE_ZWO_EAF", "OAS_ENABLE_INDI"):
        assert cv[key] == "OFF", (name, key)
for f in ("linux-aarch64-gcc.cmake", "linux-armhf-gcc.cmake", "windows-to-linux-aarch64-gcc.cmake", "windows-to-linux-armhf-gcc.cmake"):
    assert (root / "cmake" / "toolchains" / f).is_file(), f

cmake_text = (root / "CMakeLists.txt").read_text()

# Native OAL is the default everywhere; INDI requires an explicit compatibility preset.
for name in ("windows-observatory-release", "linux-observatory-release", "linux-node-release",
             "rpi4-observatory-release", "rpi4-node-release",
             "macos-observatory-release", "macos-node-release"):
    cv = cp[name].get("cacheVariables", {})
    # Some derived presets inherit the value; resolve one level through known base defaults.
    assert cv.get("OAS_ENABLE_INDI", "OFF") == "OFF", (name, cv.get("OAS_ENABLE_INDI"))
for name in ("windows-observatory-indi-release", "linux-observatory-indi-release", "linux-node-indi-release",
             "rpi4-observatory-indi-release", "rpi4-node-indi-release",
             "macos-observatory-indi-release", "macos-node-indi-release"):
    assert cp[name]["cacheVariables"]["OAS_ENABLE_INDI"] == "ON", name
assert 'option(OAS_ENABLE_INDI "Enable built-in INDI compatibility backends (optional; native OAL drivers are the default)" OFF)' in cmake_text
assert "OpenAstroLink requires Qt >= 6.4" in cmake_text
assert "if(WIN32 OR APPLE)" in cmake_text
assert "mac_arm64" in cmake_text and "mac_x64" in cmake_text
assert (root / "scripts" / "build_macos.sh").is_file()
bootstrap = (root / "scripts" / "bootstrap_rpi_observatory.sh").read_text()
for pkg in ("qt6-serialport-dev", "qt6-websockets-dev", "qt6-httpserver-dev"):
    assert pkg in bootstrap
assert "Bookworm" in bootstrap
assert "Ubuntu 24.04+" in bootstrap

for f in ("bootstrap_rpi_cross.sh", "bootstrap_qt_host.sh", "stage_qhy_cross_sdk.sh"):
    assert (root / "scripts" / f).is_file(), f
assert "OAS_QT_HOST_PATH" in cmake_text
print("build preset smoke: OK")
