#!/usr/bin/env python3
from pathlib import Path
import json, sys
root=Path(__file__).resolve().parents[1]
cpp=(root/'drivers/eqdrive/oal_driver_eqdrive.cpp').read_text(encoding='utf-8')
manifest=json.loads((root/'drivers/eqdrive/oal_driver_eqdrive.manifest.json').read_text(encoding='utf-8'))
checks=[
    ('v9 geometry capability retained','eqmod-gem-ha-dec-v9' in cpp),
    ('temporary native GOTO variable removed','gMaxNativeGotoDeg' not in cpp),
    ('temporary HIL rejection removed','HIL_SAFETY_LIMIT' not in cpp),
    ('driver config no longer exposes maxNativeGotoDeg','maxNativeGotoDeg' not in manifest.get('config',{})),
    ('raw-axis explicit mechanical safety remains','AXIS_SAFETY_LIMIT' in cpp and 'maxAxisDeltaDeg' in cpp),
    ('Core/profile safety remains separate',(root/'src/backends/oal_native_devices.cpp').read_text().find('Sky GOTO exceeds safety limit')>=0),
]
for name,ok in checks: print(('PASS' if ok else 'FAIL')+': '+name)
failed=[n for n,ok in checks if not ok]
print(f'{len(checks)-len(failed)}/{len(checks)} checks passed')
sys.exit(1 if failed else 0)
