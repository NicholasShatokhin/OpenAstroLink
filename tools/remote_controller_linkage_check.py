#!/usr/bin/env python3
from pathlib import Path
import re
root = Path(__file__).resolve().parents[1]
base = (root / 'src/core/observatory_controller.h').read_text(encoding='utf-8')
h = (root / 'src/core/remote_observatory_controller.h').read_text(encoding='utf-8')
cpp = (root / 'src/core/remote_observatory_controller.cpp').read_text(encoding='utf-8')
checks = {
    'base observationPlan pure virtual': (base, r'virtual\s+ObservationPlan\s+observationPlan\s*\(\s*\)\s*const\s*=\s*0\s*;'),
    'remote observationPlan override declaration': (h, r'ObservationPlan\s+observationPlan\s*\(\s*\)\s*const\s+override\s*;'),
    'remote observationPlan definition': (cpp, r'ObservationPlan\s+RemoteObservatoryController::observationPlan\s*\(\s*\)\s*const'),
}
failed=[]
for name,(text,pat) in checks.items():
    ok=bool(re.search(pat,text))
    print(('PASS' if ok else 'FAIL') + ': ' + name)
    if not ok: failed.append(name)
if failed:
    raise SystemExit('Missing RemoteObservatoryController linkage symbol(s): ' + ', '.join(failed))
print(f'{len(checks)}/{len(checks)} RemoteObservatoryController linkage checks PASS')
