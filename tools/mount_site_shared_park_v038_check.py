#!/usr/bin/env python3
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[1]
checks=[
 ('release version','CMakeLists.txt','project(OpenAstroSuite VERSION 0.2.10.44'),
 ('sky safety model','src/backends/oal_native_devices.cpp','Sky GOTO exceeds safety limit'),
 ('sky separation computation','src/backends/oal_native_devices.cpp','const double skySep=skySeparationDeg'),
 ('raw mechanical hard cap','src/backends/oal_native_devices.cpp','rawAxisGoto(target,180.0,e)'),
 ('preferred wire alias server','src/oal/oal_server.cpp','maxGotoSkyDeltaDeg'),
 ('preferred wire alias remote','src/core/remote_observatory_controller.cpp','maxGotoSkyDeltaDeg'),
 ('ascom auto site guard','src/core/application_controller.cpp','ensureMountBackendSiteTime'),
 ('eqmod allow site writes hint','src/core/application_controller.cpp','Allow Site Writes'),
 ('ascom site latitude write','tools/ascom_host/main.cpp','SiteLatitude'),
 ('ascom site longitude write','tools/ascom_host/main.cpp','SiteLongitude'),
 ('ascom reported azimuth','tools/ascom_host/main.cpp','azimuthDeg'),
 ('ascom reported altitude','tools/ascom_host/main.cpp','altitudeDeg'),
 ('ascom set park capability','tools/ascom_host/main.cpp','CanSetPark'),
 ('ascom SetPark call','tools/ascom_host/main.cpp','L"SetPark"'),
 ('generic park interface','src/core/interfaces.h','setCurrentParkPosition'),
 ('controller shared park','src/core/application_controller.cpp','setCurrentMountAsPark'),
 ('remote shared park','src/core/remote_observatory_controller.cpp','set-park-here'),
 ('server shared park route','src/oal/oal_server.cpp','set-park-here'),
 ('native shared home park','src/core/application_controller.cpp','Shared physical Home/Park calibrated for native mount'),
 ('mount shared park UI','src/gui/main_window.cpp','persistent Home / Park'),
 ('native-only inversion explanation','src/gui/main_window.cpp','Axis sign mapping applies only to native raw-axis mounts'),
 ('sky safety UI','src/gui/main_window.cpp','Native max sky GOTO separation'),
 ('eqmod equOther compatibility','src/backends/ascom_classic_mount.cpp','EQMOD equOther(0) compatibility'),
 ('ascom frame assumption diagnostic','src/backends/ascom_classic_mount.cpp','equatorialFrameAssumption'),
]
fail=[]
for name,file,needle in checks:
    text=(root/file).read_text(encoding='utf-8')
    if needle not in text: fail.append(f'{name}: {file} missing {needle!r}')
    else: print('PASS',name)
if fail:
    print('\n'.join('FAIL '+x for x in fail));sys.exit(1)
print(f'{len(checks)}/{len(checks)} PASS')
