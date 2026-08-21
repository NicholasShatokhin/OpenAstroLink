#!/usr/bin/env python3
from pathlib import Path
import json, sys
root=Path(__file__).resolve().parents[1]
errs=[]
def need(path,*tokens):
    p=root/path
    if not p.exists(): errs.append(f"missing {path}"); return
    s=p.read_text(errors='replace')
    for t in tokens:
        if t not in s: errs.append(f"{path}: missing {t}")
need(Path('CMakeLists.txt'),'OAS_ENABLE_NATIVE_ZWO_ASI','OAS_ENABLE_NATIVE_ZWO_EAF','oal_driver_zwo_asi','oal_driver_zwo_eaf','ZWO_ASI_INCLUDE_DIR','ZWO_EAF_INCLUDE_DIR')
need(Path('drivers/zwo_asi/oal_driver_zwo_asi.cpp'),'oal.zwo.asi','ASIGetNumOfConnectedCameras','ASIStartExposure','ASIStopExposure','publishFrame')
need(Path('drivers/zwo_eaf/oal_driver_zwo_eaf.cpp'),'oal.zwo.eaf','EAFGetNum','EAFMove','EAFStop','EAFGetTemp')
for f,driver in [('drivers/zwo_asi/oal_driver_zwo_asi.manifest.json','oal.zwo.asi'),('drivers/zwo_eaf/oal_driver_zwo_eaf.manifest.json','oal.zwo.eaf')]:
    try:
        d=json.loads((root/f).read_text());
        if d.get('driverId')!=driver or d.get('abiVersion')!=2: errs.append(f'{f}: identity/ABI mismatch')
    except Exception as e: errs.append(f'{f}: {e}')
need(Path('docs/ZWO_NATIVE.md'),'oal.zwo.asi','oal.zwo.eaf','Hardware validation required')
need(Path('docs/uk/ZWO_NATIVE.md'),'oal.zwo.asi','oal.zwo.eaf')
if errs:
    print('Native ZWO driver check: FAIL'); [print(' -',x) for x in errs]; sys.exit(1)
print('Native ZWO driver check: PASS')
