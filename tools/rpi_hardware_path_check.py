#!/usr/bin/env python3
from pathlib import Path
import json, re, sys
root=Path(__file__).resolve().parents[1]
checks=[]
def require(path, *needles):
    p=root/path
    if not p.exists():
        checks.append((False,f"missing {path}")); return
    text=p.read_text(errors='replace')
    for n in needles:
        checks.append((n in text,f"{path}: {n}"))

require(Path('CMakeLists.txt'), 'VERSION 0.2.5', 'OAS_BUILD_HARDWARE_PROBE', 'src/algorithms/astap_solver.cpp')
require(Path('src/algorithms/astap_solver.cpp'), 'OAL_ASTAP_EXECUTABLE', 'OAL_ASTAP_DATABASE', '"-fov"', '"-spd"', 'PLTSOLVD', 'CRVAL1', 'CRVAL2')
require(Path('src/backends/indi_devices.cpp'), 'EQUATORIAL_EOD_COORD', 'ON_COORD_SET', 'TELESCOPE_ABORT_MOTION', 'ABORT_MOTION', 'ABS_FOCUS_POSITION', 'FOCUS_ABORT_MOTION', 'openIndiSocket', 'short-lived client')
require(Path('src/backends/qhy_camera.cpp'), 'ensureSdkResource', 'SetQHYCCDStreamMode(handle_, 0)', 'CONTROL_TRANSFERBIT', 'SetQHYCCDParam(handle_, CONTROL_TRANSFERBIT, 16)', 'CancelQHYCCDExposingAndReadout', 'GetQHYCCDSingleFrame')
require(Path('src/tools/hardware_probe.cpp'), 'ASTAP: OK', 'INDI: OK', 'QHY: OK', 'endpoint:')
require(Path('scripts/bootstrap_rpi_observatory.sh'), 'qt6-httpserver-dev', 'indi-bin', 'libopencv-dev')
require(Path('scripts/install_indi_service.sh'), 'openastrolink-indi.service', 'indi_getprop')
require(Path('docs/RPI_FIRST_HARDWARE.md'), 'oal-hardware-probe', 'gemini-eaf', 'QHYCCD_ROOT', 'ASTAP')

presets=json.loads((root/'CMakePresets.json').read_text())
rpi=next((x for x in presets.get('configurePresets',[]) if x.get('name')=='rpi4-observatory-release'),{})
cv=rpi.get('cacheVariables',{})
checks += [
    (cv.get('OAS_ENABLE_QHY')=='ON','RPi preset QHY ON'),
    (cv.get('OAS_ENABLE_INDI')=='ON','RPi preset INDI ON'),
    (cv.get('OAS_BUILD_GUI')=='ON','RPi preset GUI ON'),
    (cv.get('OAS_BUILD_NODE')=='ON','RPi preset node ON'),
    (cv.get('OAS_BUILD_HARDWARE_PROBE')=='ON','RPi preset probe ON'),
]
# Guard against the old cross-thread persistent INDI QTcpSocket design.
h=(root/'src/backends/indi_devices.h').read_text()
checks.append(('QTcpSocket socket_' not in h,'INDI device objects do not own a persistent QTcpSocket'))
# QHY resource init should be a process singleton, not repeated in connect/scan.
q=(root/'src/backends/qhy_camera.cpp').read_text()
checks.append((q.count('InitQHYCCDResource()')==2,'QHY Init appears only in comment + singleton call'))
failed=[m for ok,m in checks if not ok]
if failed:
    print('RPi hardware path check: FAIL')
    for m in failed: print(' -',m)
    sys.exit(1)
print(f'RPi hardware path check: PASS ({len(checks)} assertions)')
