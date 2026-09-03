#!/usr/bin/env python3
from pathlib import Path
root = Path(__file__).resolve().parents[1]

cmake = (root / "CMakeLists.txt").read_text()
assert 'OAS_QT_HOST_PATH' in cmake
assert 'QT_HOST_PATH' in cmake

bootstrap = (root / "scripts" / "bootstrap_rpi_cross.sh").read_text()
for token in (
    "debootstrap", "qemu-user-static", "debian-archive-keyring", "binfmt_misc",
    "archive-key-12.asc", "release-12.asc", "archive-key-12-security.asc",
    "F8D2585B8783D481", "--keyring=",
    "update-binfmts", "/bin/true", ".oal-debootstrap-complete", "--from-pi",
    "qt6-httpserver-dev", "qt6-positioning-dev", "libopencv-dev",
    "libblas3", "liblapack3", "libblas-dev", "liblapack-dev",
    "stage_qhy_cross_sdk.sh", "bootstrap_qt_host.sh",
    'sudo symlinks -cr "$sysroot"',
    ".oal/rpi-cross-",
):
    assert token in bootstrap, token

# Regression for build-fix7: under `set -u`, Bash expands all initializers in a
# single `local` command before any of those locals are assigned.  Do not build
# `out` from `name` in the same declaration.
assert 'local name="$1" expected="$2" url="$3" out="$tmp/$name"' not in bootstrap
for token in ('local name expected url out', 'name="$1"', 'out="$tmp/$name"'):
    assert token in bootstrap, token

stage = (root / "scripts" / "stage_qhy_cross_sdk.sh").read_text()
for token in ("QHYCCD_Linux_New", "armv8", "file -Lb", "libqhy.so", "not AArch64", "QHY SDK checkout/archive"):
    assert token in stage, token

build = (root / "scripts" / "build_rpi_cross.sh").read_text()
assert "--bootstrap" in build
assert "OAS_QT_HOST_PATH" in build
assert "bootstrap_rpi_cross.sh" in build

example = (root / "CMakeUserPresets.example.json").read_text()
assert "OAS_QT_HOST_PATH" in example
assert ".local/share/openastrolink/sysroots/rpi4-arm64" in example


for tc_name, triple in (("linux-aarch64-gcc.cmake", "aarch64-linux-gnu"),
                        ("linux-armhf-gcc.cmake", "arm-linux-gnueabihf")):
    tc = (root / "cmake" / "toolchains" / tc_name).read_text()
    for token in ("CMAKE_LIBRARY_ARCHITECTURE", "Qt6_DIR", "OpenCV_DIR",
                  f"usr/lib/${{OAS_CROSS_TRIPLE}}/cmake/Qt6",
                  f"usr/lib/${{OAS_CROSS_TRIPLE}}/cmake/opencv4"):
        assert token in tc, (tc_name, token)

assert "OAS_QT_TARGET_DIR" in bootstrap
assert "OAS_OPENCV_TARGET_DIR" in bootstrap
assert "Qt target package" in build
assert "OpenCV target package" in build


# build-fix10: QHY staging is architecture-driven, not filename-driven.  The
# legacy QHYCCD_Linux_New armv8 archive is 32-bit ARM; ARM64 bootstrap may
# continue without QHY unless --require-qhy is requested.
for token in ('qhy_mode="auto"', '--require-qhy', '--qhy-source',
              "continuing with QHY disabled", 'OAS_QHY_SDK=$qhy_env'):
    assert token in bootstrap, token

for token in ('find -L', 'cp -aL', 'staged QHY SDK is missing',
              'include/qhyccd.h', 'library_file=$staged_desc',
              "candidate_name_re='(aarch64|arm64|armv8)'",
              "candidate_name_re='(arm32|armhf|armv7|rpi|armv8)'"):
    assert token in stage, token

for token in ('qhy_sdk="${OAS_QHY_SDK:-}"', 'QHY target SDK:',
              '-DQHYCCD_INCLUDE_DIR=', "preset '$preset' enables the full vendor stack",
              'ignoring stale QHY SDK record for non-full preset'):
    assert token in build, token

assert ".oal/" in (root / ".gitignore").read_text()
print("cross bootstrap smoke: OK")
