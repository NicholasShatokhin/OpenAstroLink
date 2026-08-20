#!/usr/bin/env python3
from pathlib import Path
import json, sys
root=Path(__file__).resolve().parents[1]
checks=[]
def require(path,*needles):
    p=root/path
    if not p.exists(): checks.append((False,f"missing {path}")); return
    text=p.read_text(errors='replace')
    for n in needles: checks.append((n in text,f"{path}: {n}"))

require(Path('CMakeLists.txt'),'VERSION 0.2.6','oal_driver_qhy','oal_driver_simulated','OAL_DRIVER_BUILD_DIR')
require(Path('src/algorithms/astap_solver.cpp'),'OAL_ASTAP_EXECUTABLE','OAL_ASTAP_DATABASE','"-fov"','"-spd"','PLTSOLVD','CRVAL1','CRVAL2')
require(Path('src/backends/indi_devices.cpp'),'EQUATORIAL_EOD_COORD','TELESCOPE_ABORT_MOTION','ABS_FOCUS_POSITION','FOCUS_ABORT_MOTION','short-lived client')
require(Path('drivers/qhy/oal_driver_qhy.cpp'),'InitQHYCCDResource','SetQHYCCDStreamMode(c->handle,0)','CONTROL_TRANSFERBIT','CancelQHYCCDExposingAndReadout','GetQHYCCDSingleFrame','publishFrame')
require(Path('src/tools/hardware_probe.cpp'),'Native OAL:','Native QHY: OK','INDI compatibility:','nativeBackendKey')
require(Path('scripts/install_rpi_node.sh'),'/usr/local/lib/openastrolink/drivers','*.manifest.json')
require(Path('scripts/bootstrap_rpi_observatory.sh'),'qt6-httpserver-dev','indi-bin','libopencv-dev','native ABI-v2 oal.qhy driver')
require(Path('docs/RPI_FIRST_HARDWARE.md'),'oal-hardware-probe','ASTAP')

presets=json.loads((root/'CMakePresets.json').read_text())
rpi=next((x for x in presets.get('configurePresets',[]) if x.get('name')=='rpi4-observatory-release'),{})
cv=rpi.get('cacheVariables',{})
checks += [
    (cv.get('OAS_ENABLE_QHY')=='ON','RPi preset native QHY ON'),
    (cv.get('OAS_ENABLE_INDI')=='ON','RPi preset INDI compatibility ON'),
    (cv.get('OAS_BUILD_GUI')=='ON','RPi preset GUI ON'),
    (cv.get('OAS_BUILD_NODE')=='ON','RPi preset node ON'),
    (cv.get('OAS_BUILD_HARDWARE_PROBE')=='ON','RPi preset probe ON'),
]
# Guard against old cross-thread persistent INDI socket design.
h=(root/'src/backends/indi_devices.h').read_text()
checks.append(('QTcpSocket socket_' not in h,'INDI compatibility objects do not own a persistent QTcpSocket'))
failed=[m for ok,m in checks if not ok]
if failed:
    print('RPi hardware path check: FAIL')
    for m in failed: print(' -',m)
    sys.exit(1)
print(f'RPi hardware path check: PASS ({len(checks)} assertions)')
