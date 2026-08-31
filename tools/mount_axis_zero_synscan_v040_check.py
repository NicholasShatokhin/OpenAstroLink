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

need('CMakeLists.txt','VERSION 0.2.10.44')
need('src/core/application_controller.cpp','nativeCoordinateModelVersion>=6','startup controller counts define Axis1=0°, Axis2=0°','resolvedEndpoint="EQMOD.Telescope"')
need('src/core/mount_geometry.cpp','Native/direct Sky-Watcher Motor Controller GEM model v6','gemTelescopeFrameAxesForSky','skyFromGemTelescopeFrameAxes','polarAlignedAxesForSky','shortest physical motion')
need('src/core/astro_types.h','v6 (OpenAstroLink 0.2.10.44)','controller counts present at','direction vector rotated into the polar-aligned mount frame')

eq=need('drivers/eqdrive/oal_driver_eqdrive.cpp','sessionHomeCounts1','sessionHomeCounts2','sessionHomeValid','mcHomeZeroCounts','mcCountsToMechanicalDeg','axis1ControllerCounts','axis2ControllerCounts','gem-polar-telescope-frame-v6')
assert 'countsPerRev2)/4.0' not in eq
assert 'controllerAxis1Deg' not in eq and 'controllerAxis2Deg' not in eq and 'axisNormalization' not in eq
checks.append(('eqdrive','no guessed quarter-turn / no second public controller-angle coordinate system'))
assert 'double(counts)-double(mcHomeZeroCounts(d,axis))' in eq
checks.append(('eqdrive','startup counts -> mechanical degrees'))

wifi=need('src/backends/synscan_network_mount.cpp','sessionHomeCounts1_','sessionHomeCounts2_','Direct Wi-Fi Home accepted','geometry_.axesForSky')
assert 'countsPerRev2_)/4.0' not in wifi
assert 'controllerAxis1Deg' not in wifi and 'controllerAxis2Deg' not in wifi
checks.append(('synscan-wifi','same startup-count Home convention'))

# High-level SynScan backends own their sky alignment and must not be routed
# through the direct Motor Controller mechanical-axis model.
app=need('src/backends/synscan_app_mount.cpp','RightAscensionDeclinationGet','SlewToCoordinatesAsync','SyncToCoordinates')
assert 'MountGeometryModel' not in app and 'axesForSky' not in app
checks.append(('synscan-app','high-level RA/DEC path remains independent'))
hand=need('drivers/skywatcher/oal_driver_skywatcher.cpp','mount.slew','mount.sync')
assert 'mcHomeZeroCounts' not in hand and 'nativeCoordinateModelVersion' not in hand
checks.append(('oal.skywatcher','hand-controller RA/DEC path remains independent'))

need('src/integrations/stellarium_telescope_server.cpp','positionTimer_.setInterval(250)','socket->flush()')
need('tests/mount_geometry_smoke.cpp','nativeCoordinateModelVersion=6','EQDrive v6 Home sync failed','EQDrive v6 Deneb-region target chose wrong polar-frame branch','40.9420')
print(f'mount polar-frame / SynScan v0.2.10.44: PASS ({len(checks)} assertions)')
