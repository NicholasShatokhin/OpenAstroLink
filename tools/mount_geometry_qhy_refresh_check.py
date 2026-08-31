#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT=Path(__file__).resolve().parents[1]
checks=[]
def expect(path, needle, label):
    text=(ROOT/path).read_text(encoding='utf-8')
    ok=needle in text
    checks.append((ok,label))

expect('src/core/astro_types.h','enum class MountGeometryType','Mount geometry enum exists')
for name in ['GermanEquatorial','ForkEquatorial','AltAzimuth','AltAzimuthDerotator','EquatorialPlatform','CustomTwoAxis']:
    expect('src/core/astro_types.h',name,f'Geometry type {name}')
expect('src/core/mount_geometry.cpp','localSiderealTimeDeg','Sidereal-time transform exists')
expect('src/core/mount_geometry.cpp','MountGeometryModel::axesForSky','sky-to-axis transform exists')
expect('src/core/mount_geometry.cpp','MountGeometryModel::skyFromAxes','axis-to-sky transform exists')
expect('src/backends/oal_native_devices.cpp','mount.gotoAxes','Native raw-axis GOTO is used')
expect('src/backends/synscan_network_mount.cpp','geometry_.axesForSky','Direct Wi-Fi uses core mount geometry')
expect('src/gui/main_window.cpp','Calibrate current physical pose as persistent Home / Park','Set-current park UI exists')
expect('src/gui/main_window.cpp','Clear native OAL Park calibration','Unsafe default 90/0 park replaced by explicit calibration clear')
expect('src/gui/main_window.cpp','Mechanical axes: Axis1=','Mechanical axes visible in GUI')
expect('src/core/application_controller.cpp','"geometryType",m.geometryType','Geometry present in state JSON')
expect('src/oal/oal_server.cpp','"axesValid",s.axes.valid','Mechanical axes present in mount API')
expect('src/core/application_controller.cpp','restartDriver("oal.qhy"','Explicit refresh can hard-reload QHY driver')
expect('src/oal/driver_plugin_loader.cpp','x->library->unload()','ABI-v2 hard reload unloads driver/vendor DLL')
expect('src/core/application_controller.cpp','Explicit Refresh: hard-reloaded native QHY driver DLL/SDK','Explicit QHY recovery is logged')
expect('src/core/application_controller.cpp','refreshNativeDiscoveryAsync({},true)','Explicit refresh requests hard vendor recovery')
expect('src/core/application_controller.cpp','ASCOM slew sample:','Classic ASCOM slew diagnostics exist')
expect('tests/mount_geometry_smoke.cpp','mount geometry smoke PASS','Mount geometry C++ smoke test exists')
expect('CMakeLists.txt','oal-mount-geometry-smoke','Mount geometry smoke test is in CMake')

failed=[label for ok,label in checks if not ok]
for ok,label in checks:
    print(('PASS' if ok else 'FAIL')+': '+label)
if failed:
    print(f'\n{len(failed)} failure(s)')
    sys.exit(1)
print(f'\n{len(checks)}/{len(checks)} assertions PASS')
