#!/usr/bin/env python3
from pathlib import Path
root = Path(__file__).resolve().parents[1]
cmake = (root / "CMakeLists.txt").read_text()
for token in (
    "OAL target link search:",
    "LINKER:-rpath-link,${_oas_cross_link_dir}",
    '"-L${_oas_cross_link_dir}"',
    '${OAS_CROSS_SYSROOT}/lib/${CMAKE_LIBRARY_ARCHITECTURE}',
    '${OAS_CROSS_SYSROOT}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}',
    '${OAS_CROSS_SYSROOT}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/blas',
    '${OAS_CROSS_SYSROOT}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/lapack',
    'OAL target LAPACK:',
    'OAL target BLAS:',
):
    assert token in cmake, token
bootstrap = (root / "scripts" / "bootstrap_rpi_cross.sh").read_text()
for token in ("libblas3", "liblapack3", "libblas-dev", "liblapack-dev",
              'sudo symlinks -cr "$sysroot"',
              'usr/lib/$triple/blas/libblas.so.3',
              'usr/lib/$triple/lapack/liblapack.so.3',
              'Target BLAS:', 'Target LAPACK:'):
    assert token in bootstrap, token
# Never solve this by embedding host/sysroot runtime RPATHs in deliverables.
assert "CMAKE_INSTALL_RPATH" not in cmake
print("cross linker sysroot smoke: PASS")
