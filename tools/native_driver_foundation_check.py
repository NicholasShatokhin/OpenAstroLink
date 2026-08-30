#!/usr/bin/env python3
from pathlib import Path
import json, re, subprocess, tempfile, sys
root=Path(__file__).resolve().parents[1]
checks=[]
def need(path,*tokens):
    p=root/path
    if not p.exists(): checks.append((False,f'missing {path}')); return
    t=p.read_text(errors='replace')
    for x in tokens: checks.append((x in t,f'{path}: {x}'))

need(Path('include/oal/driver_api.h'),'OAL_DRIVER_ABI_V2','OalDriverHostV2','publishFrame','emitEvent','OalDriverV2','capabilitiesJson','cancel')
need(Path('src/oal/driver_plugin_loader.cpp'),'*.manifest.json','oalCreateDriverV2','scanDefaultPaths','takePublishedFrame','ABI v2 driver has no *.manifest.json; refused','per-device-serial','bypassSerialLock','duplicate native driverId')
need(Path('src/backends/oal_native_devices.cpp'),'nativeBackendKey','NativeOalCamera','NativeOalMount','NativeOalFocuser','camera.capture','frameToken')
need(Path('src/core/application_controller.cpp'),'Native OAL driver registry','nativeBackendsFor','NativeOalCamera','Migrated legacy direct QHY binding')
need(Path('drivers/reference_simulated/oal_driver_simulated.cpp'),'oalCreateDriverV2','sim-camera','sim-mount','sim-focuser','publishFrame')
need(Path('drivers/qhy/oal_driver_qhy.cpp'),'oalCreateDriverV2','oal.qhy','publishFrame','QHYCCD SDK','minSec','maxSec','qhyccd-live','BeginQHYCCDLive','GetQHYCCDLiveFrame','StopQHYCCDLive')
need(Path('src/oal/oal_server.cpp'),'/api/v1/drivers','nativeCapabilitiesJson')
need(Path('src/gui/main_window.cpp'),'Detected native devices','Compatibility / embedded')
need(Path('schemas/driver-manifest-v2.schema.json'),'out-of-process','permissions','deviceClasses')

for m in [root/'drivers/reference_simulated/oal_driver_simulated.manifest.json',root/'drivers/qhy/oal_driver_qhy.manifest.json']:
    try:
        j=json.loads(m.read_text()); checks.append((j.get('abiVersion')==2,f'{m.name}: abiVersion=2'))
    except Exception as e: checks.append((False,f'{m.name}: {e}'))

# Native ABI must not move frame payloads through Base64 JSON.
qhy=(root/'drivers/qhy/oal_driver_qhy.cpp').read_text()
checks.append(('Base64' not in qhy and 'base64' not in qhy,'native QHY has no Base64 frame path'))
# Direct old in-core QHY backend should be gone from the build/source tree.
checks.append((not (root/'src/backends/qhy_camera.cpp').exists(),'legacy in-core QHY implementation removed'))

failed=[m for ok,m in checks if not ok]
if failed:
    print('Native driver foundation check: FAIL')
    for m in failed: print(' -',m)
    sys.exit(1)
print(f'Native driver foundation check: PASS ({len(checks)} assertions)')
