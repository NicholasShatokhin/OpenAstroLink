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

require(Path('CMakeLists.txt'),'VERSION 0.2.9','oal_driver_qhy','oal_driver_canon','oal_driver_gemini','oal_driver_skywatcher','OAL_DRIVER_BUILD_DIR')
require(Path('src/algorithms/astap_solver.cpp'),'OAL_ASTAP_EXECUTABLE','OAL_ASTAP_DATABASE','"-fov"','"-spd"','PLTSOLVD','CRVAL1','CRVAL2')
require(Path('src/backends/indi_devices.cpp'),'EQUATORIAL_EOD_COORD','TELESCOPE_ABORT_MOTION','ABS_FOCUS_POSITION','FOCUS_ABORT_MOTION','short-lived client')
require(Path('drivers/qhy/oal_driver_qhy.cpp'),'InitQHYCCDResource','SetQHYCCDStreamMode(c->handle,0)','CONTROL_TRANSFERBIT','CancelQHYCCDExposingAndReadout','GetQHYCCDSingleFrame','publishFrame')
require(Path('drivers/gemini/oal_driver_gemini.cpp'),'oal.gemini','MyFocuserPro2','OAL_GEMINI_PORT','focuser.moveAbsolute')
require(Path('drivers/skywatcher/oal_driver_skywatcher.cpp'),'oal.skywatcher','SynScan Serial Communication Protocol 3.3','OAL_SKYWATCHER_PORT','mount.slew')
require(Path('src/tools/hardware_probe.cpp'),'Native OAL:','Native QHY: OK','Native Canon EOS: OK','Native Gemini EAF:','Native Sky-Watcher:','INDI compatibility:','nativeBackendKey')
require(Path('scripts/install_rpi_node.sh'),'/usr/local/lib/openastrolink/drivers','*.manifest.json')
require(Path('scripts/bootstrap_rpi_observatory.sh'),'qt6-httpserver-dev','--with-indi','libopencv-dev','libgphoto2-dev','oal.qhy','oal.canon','oal.gemini','oal.skywatcher')
require(Path('scripts/enable_indi_compat.sh'),'indi-bin','rpi4-observatory-release','OAS_ENABLE_INDI=ON')
require(Path('scripts/copy_qhy_sdk_to_rpi.ps1'),r'C:\workspace\astro\QHYCCD_Linux',r'C:\workspace\astro\qhysdk\lib\libqhy.so.0.1.8')
require(Path('scripts/stage_qhy_sdk.sh'),'aarch64','/opt/qhyccd/include','/opt/qhyccd/lib/libqhy.so')
require(Path('docs/RPI_FIRST_HARDWARE.md'),'oal-hardware-probe','ASTAP')

presets=json.loads((root/'CMakePresets.json').read_text())
by_name={x['name']:x for x in presets.get('configurePresets',[])}
def resolved(name):
    p=by_name[name]
    out={}
    inh=p.get('inherits')
    if isinstance(inh,str): out.update(resolved(inh))
    elif isinstance(inh,list):
        for x in inh: out.update(resolved(x))
    out.update(p.get('cacheVariables',{}))
    return out
native=resolved('rpi4-native-release')
compat=resolved('rpi4-observatory-release')
checks += [
    (native.get('OAS_ENABLE_QHY')=='ON','RPi native preset QHY ON'),
    (native.get('OAS_ENABLE_NATIVE_CANON')=='ON','RPi native preset Canon ON'),
    (native.get('OAS_ENABLE_NATIVE_GEMINI')=='ON','RPi native preset Gemini ON'),
    (native.get('OAS_ENABLE_NATIVE_SKYWATCHER')=='ON','RPi native preset Sky-Watcher ON'),
    (native.get('OAS_ENABLE_INDI')=='OFF','RPi native preset INDI OFF'),
    (compat.get('OAS_ENABLE_INDI')=='ON','RPi observatory preset INDI compatibility ON'),
    (compat.get('OAS_BUILD_GUI')=='ON','RPi observatory preset GUI ON'),
    (compat.get('OAS_BUILD_NODE')=='ON','RPi observatory preset node ON'),
    (compat.get('OAS_BUILD_HARDWARE_PROBE')=='ON','RPi observatory preset probe ON'),
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
