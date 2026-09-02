#!/usr/bin/env python3
from pathlib import Path
import datetime, math
root=Path(__file__).resolve().parents[1]

def text(rel):
    return (root/rel).read_text(encoding='utf-8',errors='ignore')

def wrap180(x):
    x=x%360.0
    return x-360.0 if x>180.0 else x

def horizontal(lst, lat, ra, dec):
    ha=math.radians(wrap180(lst-ra)); de=math.radians(dec); phi=math.radians(lat)
    alt=math.asin(max(-1.0,min(1.0,math.sin(phi)*math.sin(de)+math.cos(phi)*math.cos(de)*math.cos(ha))))
    y=-math.sin(ha)*math.cos(de)
    x=math.sin(de)*math.cos(phi)-math.cos(de)*math.sin(phi)*math.cos(ha)
    az=(math.degrees(math.atan2(y,x))%360.0)
    return az,math.degrees(alt)

def sky_from_canonical(lst,a1,a2):
    if a2>=0.0:
        ha=wrap180(a1-90.0)
    else:
        ha=wrap180(a1+90.0)
    dec=90.0-abs(a2)
    return (lst-ha)%360.0,dec

app=text('src/core/application_controller.cpp')
astro=text('src/core/astro_types.h')
geom=text('src/core/mount_geometry.cpp')
test=text('tests/mount_geometry_smoke.cpp')
native=text('drivers/eqdrive/oal_driver_eqdrive.cpp')
wifi=text('src/backends/synscan_network_mount.cpp')

assert 'nativeCoordinateModelVersion>=9' in app
assert 'profile_.mount.nativeCoordinateModelVersion=9' in app
assert 'profile_.mount.axis1Sign=1' in app
assert 'profile_.mount.axis2Sign=-1' in app
assert 'DEC/polar-distance mirror fix' in app
assert 'v9 (OpenAstroLink 0.2.10.47 build-fix 3)' in astro
assert 'Direct Sky-Watcher/EQDrive GEM model v7-v9' in geom
assert 'nativeCoordinateModelVersion>=7' in geom  # v9 intentionally reuses v7 branch equations
assert 'EQDrive v9 follow-up HIL transform failed' in test
assert '11.116419' in test and '67.847360' in test
assert 'eqmod-gem-ha-dec-v9' in native

# Low-level serial and UDP transports stay polarity-neutral; installation
# mapping belongs to Core geometry only.
for bad in ('transportAxisSign','transportAxis1Sign','transportAxis2Sign','transportPolarity'):
    assert bad not in wifi, bad
assert 'makeGotoPlan(deltaDeg,cpr)' in native
assert 'makeGotoPlan(deltaDeg,cpr)' in wifi

# Exact follow-up HIL target from 2026-09-02 08:33:47.136 UTC.
dt=datetime.datetime.fromisoformat('2026-09-02T08:33:47.136+00:00')
jd=2440587.5+dt.timestamp()/86400.0
T=(jd-2451545.0)/36525.0
gmst=280.46061837+360.98564736629*(jd-2451545.0)+0.000387933*T*T-(T*T*T)/38710000.0
lst=(gmst+30.496667)%360.0
ra,dec=61.569978,22.152640
ha=wrap180(lst-ra)
assert ha>0.0
canonical_axis1=wrap180(ha-90.0)
canonical_axis2=-(90.0-dec)
assert abs(lst-140.453559)<2e-4,lst
assert abs(canonical_axis1+11.116419)<2e-3,canonical_axis1
assert abs(canonical_axis2+67.847360)<1e-6,canonical_axis2

# Branch A is the short EQMOD east state. Branch B is mathematically equivalent
# but requires ~169 deg of Axis1 travel from Home and is not the intended route.
branch_b_axis1=wrap180(canonical_axis1+180.0)
branch_b_axis2=-canonical_axis2
bra,bdec=sky_from_canonical(lst,branch_b_axis1,branch_b_axis2)
assert abs(wrap180(bra-ra))<1e-6 and abs(bdec-dec)<1e-6
assert abs(branch_b_axis1)>160.0

# The raw axes actually reached by build-fix2 were +11.1142,-67.8473 deg.
# Reinterpret those observed raw deltas under all four possible physical sign
# mappings. Only an Axis2 reversal produces the measured exact east/west mirror:
# desired Az ~=274.64, mirrored Az ~=85.36 at the same altitude.
raw1,raw2=11.11421875,-67.84734375
lat=54.476389
variants={}
for s1,s2 in ((1,1),(-1,1),(1,-1),(-1,-1)):
    c1,c2=raw1/s1,raw2/s2
    vra,vdec=sky_from_canonical(140.644213,c1,c2)
    variants[(s1,s2)]=horizontal(140.644213,lat,vra,vdec)
assert abs(variants[(-1,1)][0]-274.64184)<0.01  # what OAL v8 *believed*
assert abs(variants[(1,-1)][0]-85.35816)<0.01   # actual exact mirror if hardware is +1/-1
assert abs(variants[(-1,1)][1]-variants[(1,-1)][1])<1e-6

# Therefore the command needed for the desired target on this hardware is
# raw = (+1,-1) * canonical = -11.1164,+67.8474 deg.
physical_axis1=canonical_axis1
physical_axis2=-canonical_axis2
assert abs(physical_axis1+11.116419)<2e-3
assert abs(physical_axis2-67.847360)<1e-6
counts_per_rev=9216000.0
assert abs(physical_axis1*counts_per_rev/360.0 + 284580.3)<80.0
assert abs(physical_axis2*counts_per_rev/360.0 - 1736892.4)<5.0
print('direct-MC Axis2 east/west mirror build-fix 3: PASS')
