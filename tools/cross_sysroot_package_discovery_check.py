#!/usr/bin/env python3
from pathlib import Path
import subprocess, tempfile
root = Path(__file__).resolve().parents[1]
for toolchain, triple in [("linux-aarch64-gcc.cmake", "aarch64-linux-gnu"), ("linux-armhf-gcc.cmake", "arm-linux-gnueabihf")]:
    with tempfile.TemporaryDirectory(prefix="oal-cross-sysroot-") as td:
        sysroot = Path(td) / "sysroot"
        qt = sysroot / "usr" / "lib" / triple / "cmake" / "Qt6"
        cv = sysroot / "usr" / "lib" / triple / "cmake" / "opencv4"
        qt.mkdir(parents=True); cv.mkdir(parents=True)
        (qt / "Qt6Config.cmake").write_text("# fake target Qt config\n")
        (cv / "OpenCVConfig.cmake").write_text("# fake target OpenCV config\n")
        probe = Path(td) / "probe.cmake"
        probe.write_text(
            f'include("{(root / "cmake" / "toolchains" / toolchain).as_posix()}")\n'
            'message(STATUS "Qt6_DIR=${Qt6_DIR}")\n'
            'message(STATUS "OpenCV_DIR=${OpenCV_DIR}")\n'
            'message(STATUS "CMAKE_LIBRARY_ARCHITECTURE=${CMAKE_LIBRARY_ARCHITECTURE}")\n')
        out = subprocess.check_output(["cmake", f"-DOAS_CROSS_SYSROOT={sysroot}", "-P", str(probe)], text=True, stderr=subprocess.STDOUT)
        assert f"Qt6_DIR={qt}" in out, out
        assert f"OpenCV_DIR={cv}" in out, out
        assert f"CMAKE_LIBRARY_ARCHITECTURE={triple}" in out, out
print("cross sysroot package discovery: PASS")
