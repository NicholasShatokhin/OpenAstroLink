#!/usr/bin/env python3
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[1]
checks={
    'mount checkbox members': ('src/gui/main_window.h','QCheckBox *mountTracking_{}; QCheckBox *mountParked_{};'),
    'mount signal blocker': ('src/gui/main_window.cpp','QSignalBlocker b(mountTracking_)'),
    'focuser dirty target': ('src/gui/main_window.cpp','!focusTargetDirty_'),
    'actual focuser label': ('src/gui/main_window.cpp','Actual position:'),
    'autofocus busy guard': ('src/gui/main_window.cpp','setAutofocusBusy(true)'),
    'tabs disabled during af': ('src/gui/main_window.cpp','tabs_->setEnabled(!busy)'),
    'mount pier snapshot': ('src/core/application_controller.cpp','{"pierSide",m.pierSide}'),
    'fresh control connections': ('src/backends/http_json_client.cpp','setRawHeader("Connection","close")'),
}
failed=[]
for name,(file,needle) in checks.items():
    text=(root/file).read_text(encoding='utf-8')
    if needle not in text: failed.append(f'{name}: missing {needle!r} in {file}')
if failed:
    print('\n'.join(failed));sys.exit(1)
print('Runtime state/UI hotfix checks passed.')
