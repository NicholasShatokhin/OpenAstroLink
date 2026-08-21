#!/usr/bin/env python3
from pathlib import Path
root = Path(__file__).resolve().parents[1]
cmake = (root / 'CMakeLists.txt').read_text(encoding='utf-8')
driver = (root / 'drivers/qhy/oal_driver_qhy.cpp').read_text(encoding='utf-8')
tmpl = root / 'cmake/qhyccd_sdk_include.h.in'
assert tmpl.exists(), 'missing QHY absolute include wrapper template'
assert '${QHYCCD_INCLUDE_DIR})' not in cmake.split('add_library(oal_driver_qhy',1)[1].split('endif()',1)[0], 'QHY include dir still leaks into target include path'
assert 'qhyccd_sdk_include.h' in driver
assert '#include <qhyccd.h>' not in driver
assert 'file(TO_CMAKE_PATH "${QHYCCD_INCLUDE_DIR}/qhyccd.h" OAL_QHYCCD_HEADER)' in cmake
assert 'NOMINMAX WIN32_LEAN_AND_MEAN' in cmake
print('qhy_header_isolation_check: PASS')
