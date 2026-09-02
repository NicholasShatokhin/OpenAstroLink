#!/usr/bin/env python3
from pathlib import Path
import datetime
root=Path(__file__).resolve().parents[1]

def text(rel):
    return (root/rel).read_text(encoding='utf-8',errors='ignore')

app=text('src/core/application_controller.cpp')
astro=text('src/core/astro_types.h')
geom=text('src/core/mount_geometry.cpp')
test=text('tests/mount_geometry_smoke.cpp')
native=text('drivers/eqdrive/oal_driver_eqdrive.cpp')
wifi=text('src/backends/synscan_network_mount.cpp')

assert 'nativeCoordinateModelVersion>=8' in app
assert 'profile_.mount.nativeCoordinateModelVersion=8' in app
assert 'profile_.mount.axis1Sign=-1' in app
assert 'profile_.mount.axis2Sign=1' in app
assert '2026-09-02 east/west mirror HIL' in app
assert 'v8 (OpenAstroLink 0.2.10.47 build-fix 2)' in astro
assert 'Direct Sky-Watcher/EQDrive GEM model v7/v8' in geom
assert 'nativeCoordinateModelVersion>=7' in geom  # v8 intentionally reuses v7 branch equations
assert 'EQDrive v8 mirror HIL transform failed' in test
assert '1.935146' in test and '69.911983' in test
assert 'eqmod-gem-ha-dec-v8' in native

# Low-level serial and UDP transports stay polarity-neutral; the physical
# installation mapping belongs to Core geometry only.
for bad in ('transportAxisSign','transportAxis1Sign','transportAxis2Sign','transportPolarity'):
    assert bad not in wifi, bad
assert 'makeGotoPlan(deltaDeg,cpr)' in native
assert 'makeGotoPlan(deltaDeg,cpr)' in wifi

# Exact ASCOM-correct HIL target from 2026-09-02 07:55:55.239 UTC.
dt=datetime.datetime.fromisoformat('2026-09-02T07:55:55.239+00:00')
jd=2440587.5+dt.timestamp()/86400.0
T=(jd-2451545.0)/36525.0
gmst=280.46061837+360.98564736629*(jd-2451545.0)+0.000387933*T*T-(T*T*T)/38710000.0
lst=(gmst+30.496667)%360.0
ra,dec=39.026257,20.088017
ha=(lst-ra+180.0)%360.0-180.0
assert ha>0.0
canonical_axis1=((ha-90.0+180.0)%360.0)-180.0
canonical_axis2=-(90.0-dec)
physical_axis1=-canonical_axis1
physical_axis2=canonical_axis2
assert abs(lst-130.9614037)<1e-4,lst
assert abs(canonical_axis1-1.9351467)<1e-3,canonical_axis1
assert abs(canonical_axis2+69.911983)<1e-6,canonical_axis2
assert abs(physical_axis1+1.9351467)<1e-3,physical_axis1
assert abs(physical_axis2+69.911983)<1e-6,physical_axis2
print('direct-MC Axis1 mirror build-fix 2: PASS')
