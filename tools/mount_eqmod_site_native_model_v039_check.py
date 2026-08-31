#!/usr/bin/env python3
from pathlib import Path
import math, sys
root=Path(__file__).resolve().parents[1]
checks=[]
def need(path,*tokens):
    text=(root/path).read_text(encoding='utf-8')
    for token in tokens:
        if token not in text:
            raise SystemExit(f'FAIL {path}: missing {token!r}')
        checks.append((path,token))
    return text

need('CMakeLists.txt','project(OpenAstroSuite VERSION 0.2.10.44')
need('src/core/astro_types.h','nativeCoordinateModelVersion','preferBackendSite')
need('src/core/settings.cpp','mountGeometry/nativeCoordinateModelVersion','mountGeometry/preferBackendSite')
need('src/core/mount_geometry.cpp','gemTelescopeFrameAxesForSky','skyFromGemTelescopeFrameAxes','nativeCoordinateModelVersion>=6','canonical={0.0,0.0,true}')
need('src/backends/oal_native_devices.cpp','standardEqDriveHome','eqdrive-zero-home','polar-home-sync','standardEqDrivePark','Near-pole Sync at Home')
need('src/core/application_controller.cpp','coordinate model migrated to v6','profile_.mount.axis1Sign=1','profile_.mount.axis2Sign=1','profile_.mount.autoHomeSync=true','ASCOM backend site adopted into OpenAstroLink profile','resolvedEndpoint="EQMOD.Telescope"')
need('src/core/remote_observatory_controller.cpp','nativeCoordinateModelVersion','preferBackendSite','refreshMetadata(nullptr);emit profileChanged()')
need('src/oal/oal_server.cpp','nativeCoordinateModelVersion','preferBackendSite')
need('src/gui/main_window.cpp','use mount/EQMOD site as authoritative','EQMOD.Telescope')
need('tests/mount_geometry_smoke.cpp','EQDrive v6 HIL target transform failed','51.9348','15.9484')

# Independent numeric reproduction of the v6 polar telescope-frame convention
# for the 2026-08-31 HIL target.  The logged target horizontal coordinates were
# Az=24.206042°, Alt=58.155869° at latitude 50.476481°. Rotate that horizon
# vector into the polar-aligned mount frame exactly as the production model does.
lat=math.radians(50.476481); az=math.radians(24.206042); alt=math.radians(58.155869)
x=math.cos(alt)*math.cos(az); y=-math.cos(alt)*math.sin(az); z=math.sin(alt)
r=math.radians(50.476481-90.0); c=math.cos(r); sn=math.sin(r)
rx=c*x+sn*z; ry=y; rz=-sn*x+c*z
mount_az=(math.degrees(math.atan2(-ry,rx))%360.0)
mount_alt=math.degrees(math.atan2(rz,math.hypot(rx,ry)))
q=((mount_az+180.0)%360.0)-180.0; p=90.0-mount_alt
alt_branch=(((q+180.0)+180.0)%360.0)-180.0, -p
axis1,axis2=min([(q,p),alt_branch],key=lambda a:max(abs(a[0]),abs(a[1]))+0.05*(abs(a[0])+abs(a[1])))
assert abs(axis1+51.934804)<1e-3,(axis1,axis2)
assert abs(axis2+15.948403)<1e-3,(axis1,axis2)
checks += [('numeric','polar telescope-frame Axis1 branch'),('numeric','polar telescope-frame signed polar distance')]
print(f'{len(checks)}/{len(checks)} PASS')
