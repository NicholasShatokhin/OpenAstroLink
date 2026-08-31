from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
h = (root / 'src/gui/main_window.h').read_text(encoding='utf-8')
c = (root / 'src/gui/main_window.cpp').read_text(encoding='utf-8')

# Every private/public MainWindow method declared with these ordinary return types
# must have an out-of-class definition. This catches linker-only regressions such
# as v0.2.10.21's missing refreshFocuserStatus().
declared = set(re.findall(r'\b(?:void|QWidget\s*\*)\s+(\w+)\s*\(', h))
defined = set(re.findall(r'MainWindow::(\w+)\s*\(', c))
missing = sorted(declared - defined)
assert not missing, f'MainWindow methods declared but not defined: {missing}'
assert 'void MainWindow::refreshFocuserStatus()' in c
assert 'VERSION 0.2.10.45' in (root / 'CMakeLists.txt').read_text(encoding='utf-8')
print(f'PASS: MainWindow link contract ({len(declared)} methods checked)')
