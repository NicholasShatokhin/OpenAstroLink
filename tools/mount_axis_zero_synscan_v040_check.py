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
need('CMakeLists.txt','VERSION 0.2.10.45')
need('src/core/application_controller.cpp','nativeCoordinateModelVersion>=7','startup controller counts define Axis1=0°, Axis2=0°','resolvedEndpoint="EQMOD.Telescope"')
need('src/core/mount_geometry.cpp','Direct Sky-Watcher/EQDrive GEM model v7','eqmodGemAxesForSky','skyFromEqmodGemAxes','physical pointing state / side-of-pier')
need('src/core/astro_types.h','v7 (OpenAstroLink 0.2.10.45)','HA=-6h, Dec=+90°')
eq=need('drivers/eqdrive/oal_driver_eqdrive.cpp','sessionHomeCounts1','sessionHomeCounts2','sessionHomeValid','mcHomeZeroCounts','mcCountsToMechanicalDeg','axis1ControllerCounts','axis2ControllerCounts','eqmod-gem-ha-dec-v7')
assert 'countsPerRev2)/4.0' not in eq
assert 'double(counts)-double(mcHomeZeroCounts(d,axis))' in eq
checks.append(('eqdrive','startup counts -> mechanical degrees'))
wifi=need('src/backends/synscan_network_mount.cpp','sessionHomeCounts1_','sessionHomeCounts2_','Direct Wi-Fi Home accepted','geometry_.axesForSky')
assert 'countsPerRev2_)/4.0' not in wifi
checks.append(('synscan-wifi','same startup-count Home convention'))
need('tests/mount_geometry_smoke.cpp','nativeCoordinateModelVersion=7','EQDrive v7 Home sync failed','EQDrive v7 northern Home must use EQMOD west branch','44.307735')
print(f'mount Home/SynScan EQMOD v0.2.10.45: PASS ({len(checks)} assertions)')
