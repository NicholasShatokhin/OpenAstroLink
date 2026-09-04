#!/usr/bin/env python3
import json
from pathlib import Path
root = Path(__file__).resolve().parents[1]
d = json.loads((root / 'CMakePresets.json').read_text())
cp = {p['name']: p for p in d['configurePresets']}
bp = {p['name']: p for p in d['buildPresets']}
msvc = cp['windows-msvc-release']
assert msvc['generator'] == 'Ninja'
assert msvc['cacheVariables']['CMAKE_CXX_COMPILER'].lower() == 'cl.exe'
assert msvc['cacheVariables']['OAS_REQUIRE_MSVC'] == 'ON'
assert 'architecture' not in msvc
for name in ('windows-core-release','windows-native-release'):
    assert cp[name]['inherits'] == 'windows-msvc-release', (name, cp[name]['inherits'])
for name in ('windows-core-release','windows-native-release','windows-observatory-release','windows-observatory-indi-release'):
    assert 'configuration' not in bp[name], name
assert cp['windows-observatory-release']['binaryDir'].endswith('windows-observatory-msvc-ninja')
assert cp['windows-observatory-indi-release']['binaryDir'].endswith('windows-observatory-indi-msvc-ninja')
text = (root / 'scripts' / 'build_windows.ps1').read_text()
assert 'Ensure-Ninja' in text
assert 'Import-VcVars64' in text
assert 'Resolve-PresetBinaryDir' in text
assert 'windows-observatory-msvc-ninja' in text
print('windows MSVC/Ninja preset smoke: OK')
