#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parents[1]
checks=[]
def need(path,*tokens):
    text=(root/path).read_text(encoding='utf-8',errors='ignore')
    for token in tokens:
        assert token in text,f'{path}: missing {token!r}'
        checks.append((path,token))
    return text
need('CMakeLists.txt','project(OpenAstroSuite VERSION 0.2.10.51')
need('src/core/astro_types.h','nativeCoordinateModelVersion','preferBackendSite','HA=-6h, Dec=+90°')
need('src/core/settings.cpp','mountGeometry/nativeCoordinateModelVersion','mountGeometry/preferBackendSite')
need('src/core/mount_geometry.cpp','eqmodGemAxesForSky','skyFromEqmodGemAxes','nativeCoordinateModelVersion>=7','canonical={0.0,0.0,true}')
need('src/backends/oal_native_devices.cpp','standardEqDriveHome','eqdrive-zero-home','standardEqDrivePark')
need('src/core/application_controller.cpp','coordinate model migrated to v9','profile_.mount.axis1Sign=1','profile_.mount.axis2Sign=-1','profile_.mount.autoHomeSync=true','ASCOM backend site adopted into OpenAstroLink profile','resolvedEndpoint="EQMOD.Telescope"')
need('src/core/remote_observatory_controller.cpp','nativeCoordinateModelVersion','preferBackendSite','refreshMetadata(nullptr);emit profileChanged()')
need('src/oal/oal_server.cpp','nativeCoordinateModelVersion','preferBackendSite')
need('src/gui/main_window.cpp','use mount/EQMOD site as authoritative','EQMOD.Telescope')
need('tests/mount_geometry_smoke.cpp','EQDrive v9 Moon transform failed','44.307735','77.299641','11.116419','67.847360')
print(f'{len(checks)}/{len(checks)} PASS')
