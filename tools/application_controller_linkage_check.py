#!/usr/bin/env python3
from pathlib import Path
import re
root = Path(__file__).resolve().parents[1]
h = (root / 'src/core/application_controller.h').read_text(encoding='utf-8')
cpp = (root / 'src/core/application_controller.cpp').read_text(encoding='utf-8')
checks = {
    'disconnectCamera declaration': r'bool\s+disconnectCamera\s*\(QString\s*\*error\s*=\s*nullptr\)\s*override\s*;',
    'disconnectGuideCamera declaration': r'bool\s+disconnectGuideCamera\s*\(QString\s*\*error\s*=\s*nullptr\)\s*override\s*;',
    'disconnectCamera definition': r'bool\s+ApplicationController::disconnectCamera\s*\(QString\s*\*\s*error\s*\)',
    'disconnectGuideCamera definition': r'bool\s+ApplicationController::disconnectGuideCamera\s*\(QString\s*\*\s*error\s*\)',
}
failed=[]
for name, pat in checks.items():
    text = h if 'declaration' in name else cpp
    ok = bool(re.search(pat, text))
    print(('PASS' if ok else 'FAIL') + ': ' + name)
    if not ok: failed.append(name)
if failed:
    raise SystemExit('Missing ApplicationController linkage symbol(s): ' + ', '.join(failed))
print(f'{len(checks)}/{len(checks)} ApplicationController linkage checks PASS')
