#!/usr/bin/env python3
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[1]
checks=[
 ('release version','CMakeLists.txt','project(OpenAstroSuite VERSION 0.2.10.50'),
 ('SER writer source','src/core/ser_writer.cpp','LUCAM-RECORDER'),
 ('SER raw pre-debayer order','src/core/application_controller.cpp','appendSer(frame,serError)'),
 ('SER live request','src/core/astro_types.h','bool recordSer{false}'),
 ('SER UI','src/gui/main_window.cpp','Record raw Live View to SER'),
 ('Mil-Dot overlay','src/gui/main_window.cpp','Mil-Dot'),
 ('angular grid overlay','src/gui/main_window.cpp','Angular measurement grid'),
 ('tracking rate model','src/core/astro_types.h','enum class TrackingRate'),
 ('lunar tracking UI','src/gui/main_window.cpp','Lunar / Moon'),
 ('solar tracking UI','src/gui/main_window.cpp','Solar / Sun'),
 ('EQDrive lunar rate','drivers/eqdrive/oal_driver_eqdrive.cpp','14.492052'),
 ('ASCOM coordinate valid','src/backends/ascom_classic_mount.cpp','s.coordinateValid=true'),
 ('ASCOM backend diagnostics','tools/ascom_host/main.cpp','siteLatitude'),
 ('ASCOM site time setter','tools/ascom_host/main.cpp','setSiteTime'),
 ('mount site/time API','src/core/observatory_controller.h','setMountSiteTime'),
 ('site/time GUI apply','src/gui/main_window.cpp','Apply site/time to mount backend'),
 ('configurable raw-axis safety','src/backends/oal_native_devices.cpp','maxGotoAxisDeltaDeg'),
 ('sky-separation safety GUI','src/gui/main_window.cpp','Native max sky GOTO separation'),
 ('sky separation diagnostics','src/core/application_controller.cpp','skySeparation='),
 ('near pole sync warning','src/core/application_controller.cpp','weak RA/axis-1 calibration anchor'),
 ('mount backend diagnostic snapshot','src/core/application_controller.cpp','Mount diagnostic CONNECT'),
 ('ASCOM diagnostics state','src/core/application_controller.cpp','mj["diagnostics"]'),
 ('remote mount diagnostics','src/core/remote_observatory_controller.cpp','s.diagnostics=o.value("diagnostics").toObject()'),
 ('live preview stale-cache avoidance','src/core/remote_observatory_controller.cpp','live?QString("latest"):id'),
]
fail=[]
for name,file,needle in checks:
    text=(root/file).read_text(encoding='utf-8')
    if needle not in text: fail.append(f'{name}: {file} missing {needle!r}')
    else: print('PASS',name)
if fail:
    print('\n'.join('FAIL '+x for x in fail));sys.exit(1)
print(f'{len(checks)}/{len(checks)} PASS')
