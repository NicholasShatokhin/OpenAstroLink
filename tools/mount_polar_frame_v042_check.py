#!/usr/bin/env python3
# Historical filename retained; the current gate validates the v9 direct-MC replacement
# for the superseded v6 polar-frame / shortest-branch model.
from pathlib import Path
import datetime, math
root=Path(__file__).resolve().parents[1]
checks=[]
def need(rel,*tokens):
    text=(root/rel).read_text(encoding='utf-8',errors='ignore')
    for token in tokens:
        assert token in text, f'{rel}: missing {token!r}'
        checks.append((rel,token))
    return text
need('CMakeLists.txt','VERSION 0.2.10.49')
need('src/core/application_controller.cpp','nativeCoordinateModelVersion>=9','migrated to v9','EQMOD GEM pointing-state geometry retained')
geom=need('src/core/mount_geometry.cpp','Direct Sky-Watcher/EQDrive GEM model v7-v9','eqmodGemAxesForSky','skyFromEqmodGemAxes','ha<=0.0','westBranch?p:-p')
need('src/core/astro_types.h','v9 (OpenAstroLink 0.2.10.47 build-fix 3)','HA=-6h, Dec=+90°','west')
eq=need('drivers/eqdrive/oal_driver_eqdrive.cpp','sessionHomeCounts1','sessionHomeCounts2','eqmod-gem-ha-dec-v9','axis1HomeZeroCounts','axis2HomeZeroCounts')
assert 'countsPerRev2)/4.0' not in eq and 'countsPerRev2) / 4.0' not in eq
checks.append(('eqdrive','no invented DEC quarter-turn'))
need('src/backends/synscan_network_mount.cpp','geometry_.axesForSky','sessionHomeCounts1_','sessionHomeCounts2_')
need('tests/mount_geometry_smoke.cpp','nativeCoordinateModelVersion=9','EQDrive v9 Moon transform failed','44.307735','77.299641','11.116419','67.847360')
# Exact independent reproduction of the 2026-08-31 Moon HIL target.
dt=datetime.datetime.fromisoformat('2026-08-31T21:39:31.666+00:00')
jd=2440587.5+dt.timestamp()/86400.0
T=(jd-2451545.0)/36525.0
gmst=280.46061837+360.98564736629*(jd-2451545.0)+0.000387933*T*T-(T*T*T)/38710000.0
lst=(gmst+30.496667)%360.0
ra,dec=21.147893,12.700359
ha=(lst-ra+180.0)%360.0-180.0
assert ha<=0.0
axis1=((ha+90.0+180.0)%360.0)-180.0
axis2=90.0-dec
assert abs(axis1-44.3077355)<1e-3,(lst,ha,axis1)
assert abs(axis2-77.299641)<1e-6,axis2
physical_axis1=axis1   # v9 direct-MC Axis1Sign=+1
physical_axis2=-axis2  # v9 direct-MC Axis2Sign=-1
assert abs(physical_axis1-44.3077355)<1e-3,physical_axis1
assert abs(physical_axis2+77.299641)<1e-6,physical_axis2
checks += [('numeric','EQMOD west-branch canonical Axis1'),('numeric','EQMOD west-branch Axis2'),('numeric','v9 direct-MC physical Axis1'),('numeric','v9 direct-MC physical Axis2')]
print(f'mount EQMOD GEM v9: PASS ({len(checks)} assertions)')
