#!/usr/bin/env python3
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[1]
checks={
    'mount checkbox members': ('src/gui/main_window.h','QCheckBox *mountTracking_{}; QCheckBox *mountParked_{};'),
    'mount signal blocker': ('src/gui/main_window.cpp','QSignalBlocker b(mountTracking_)'),
    'focuser dirty target': ('src/gui/main_window.cpp','!focusTargetDirty_'),
    'actual focuser label': ('src/gui/main_window.cpp','Actual position:'),
    'async autofocus start': ('src/gui/main_window.cpp','startAutofocus(r,&e)'),
    'autofocus cancel action': ('src/gui/main_window.cpp','cancelOperation(autofocusOperationId_'),
    'mount remains available during autofocus': ('src/gui/main_window.cpp','mount controls remain available'),
    'mount pier snapshot': ('src/core/application_controller.cpp','{"pierSide",m.pierSide}'),
    'fresh control connections': ('src/backends/http_json_client.cpp','setRawHeader("Connection","close")'),
    'operations tab': ('src/gui/main_window.cpp','buildOperationsTab()'),
}
failed=[]
for name,(file,needle) in checks.items():
    text=(root/file).read_text(encoding='utf-8')
    if needle not in text: failed.append(f'{name}: missing {needle!r} in {file}')
if 'tabs_->setEnabled(!busy)' in (root/'src/gui/main_window.cpp').read_text(encoding='utf-8'):
    failed.append('async UI regression: whole tab set is still disabled during autofocus')
if failed:
    print('\n'.join(failed));sys.exit(1)
print('Runtime state/UI async-operation checks passed.')
