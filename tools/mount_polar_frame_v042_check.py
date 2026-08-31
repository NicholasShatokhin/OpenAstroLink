#!/usr/bin/env python3
from pathlib import Path
import math
root=Path(__file__).resolve().parents[1]
checks=[]
def need(rel,*tokens):
    text=(root/rel).read_text(encoding='utf-8',errors='ignore')
    for token in tokens:
        assert token in text, f'{rel}: missing {token!r}'
        checks.append((rel,token))
    return text

need('CMakeLists.txt','VERSION 0.2.10.44')
need('src/core/application_controller.cpp','nativeCoordinateModelVersion>=6','migrated to v6','polar-aligned telescope-vector transform')
geom=need('src/core/mount_geometry.cpp','Native/direct Sky-Watcher Motor Controller GEM model v6','gemTelescopeFrameAxesForSky','skyFromGemTelescopeFrameAxes','polarAlignedAxesForSky','90.0-spherical.axis2Deg')
assert 'if(config_.nativeCoordinateModelVersion>=6)' in geom
checks.append(('mount_geometry','v6 dispatch'))
need('drivers/skywatcher/motor_controller_protocol.h','0x800000','decodePosition')
eq=need('drivers/eqdrive/oal_driver_eqdrive.cpp','sessionHomeCounts1','sessionHomeCounts2','gem-polar-telescope-frame-v6','axis1HomeZeroCounts','axis2HomeZeroCounts')
assert 'countsPerRev2)/4.0' not in eq and 'countsPerRev2) / 4.0' not in eq
checks.append(('eqdrive','no invented DEC quarter-turn'))
need('src/backends/synscan_network_mount.cpp','geometry_.axesForSky','sessionHomeCounts1_','sessionHomeCounts2_')
app=need('src/backends/synscan_app_mount.cpp','SlewToCoordinatesAsync','SyncToCoordinates')
assert 'MountGeometryModel' not in app
checks.append(('synscan-app','high-level sky model independent'))
hand=need('drivers/skywatcher/oal_driver_skywatcher.cpp','mount.slew','mount.sync')
assert 'gemTelescopeFrameAxesForSky' not in hand
checks.append(('oal.skywatcher','hand-controller sky model independent'))
need('src/integrations/stellarium_telescope_server.cpp','positionTimer_.setInterval(250)')
need('tests/mount_geometry_smoke.cpp','nativeCoordinateModelVersion=6','EQDrive v6 Deneb-region target chose wrong polar-frame branch','40.9420','44.6201')

# Independent reproduction of the v6 transform for the latest HIL target.
# Target horizontal position from the node log: Az=92.270944°, Alt=62.571424°
# at latitude 54.476389°. Rotate horizon vector by lat-90 into mount frame.
lat=54.476389; az=92.270944; alt=62.571424
r=math.pi/180.0
a=az*r; h=alt*r
x=math.cos(h)*math.cos(a); y=-math.cos(h)*math.sin(a); z=math.sin(h)
rot=(lat-90.0)*r; c=math.cos(rot); s=math.sin(rot)
rx=c*x+s*z; ry=y; rz=-s*x+c*z
mount_az=math.degrees(math.atan2(-ry,rx))%360.0
mount_alt=math.degrees(math.atan2(rz,math.hypot(rx,ry)))
q=((mount_az+180.0)%360.0)-180.0
p=90.0-mount_alt
branches=[(q,p),((((q+180.0)+180.0)%360.0)-180.0,-p)]
axis1,axis2=min(branches,key=lambda t:max(abs(t[0]),abs(t[1]))+0.05*(abs(t[0])+abs(t[1])))
assert abs(axis1+40.941954)<1e-3,(axis1,axis2)
assert abs(axis2+44.620140)<1e-3,(axis1,axis2)
checks += [('numeric','latest HIL shortest Axis1 branch'),('numeric','latest HIL signed polar-distance branch')]
print(f'mount polar telescope-frame v0.2.10.44: PASS ({len(checks)} assertions)')
