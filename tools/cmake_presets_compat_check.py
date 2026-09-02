#!/usr/bin/env python3
from pathlib import Path
import json

root = Path(__file__).resolve().parents[1]
repo = json.loads((root / 'CMakePresets.json').read_text(encoding='utf-8'))
example = json.loads((root / 'CMakeUserPresets.example.json').read_text(encoding='utf-8'))
assert repo.get('version') == 2, f"repository preset schema must be v2, got {repo.get('version')}"
assert example.get('version') == 2, f"user preset example schema must be v2, got {example.get('version')}"

cmake_text = (root / 'CMakeLists.txt').read_text(encoding='utf-8')
assert 'cmake_minimum_required(VERSION 3.20)' in cmake_text
assert 'VERSION 0.2.10.48' in cmake_text

presets = {p['name']: p for p in repo['configurePresets']}
for name in ('windows-core-release','windows-native-release','windows-observatory-release'):
    p = presets[name]
    # Resolve generator through inheritance sufficiently for this repository.
    gen = p.get('generator')
    parent = p.get('inherits')
    if not gen and isinstance(parent, str):
        gen = presets[parent].get('generator')
        if not gen and presets[parent].get('inherits'):
            gen = presets[presets[parent]['inherits']].get('generator')
    assert gen == 'Ninja', f'{name}: expected Ninja, got {gen}'
    cache = {}
    chain=[]; cur=name
    while cur:
        chain.append(cur)
        q=presets[cur]
        par=q.get('inherits')
        cur=par if isinstance(par,str) else None
    for n in reversed(chain): cache.update(presets[n].get('cacheVariables',{}))
    assert cache.get('OAS_REQUIRE_MSVC') == 'ON', f'{name}: must require MSVC'

readme=(root/'README.md').read_text(encoding='utf-8')
assert 'CMake >= 3.20' in readme
assert 'Unrecognized "version" field' in readme
assert 'my-windows-observatory-edsdk' in readme
print('cmake_presets_compat_check: PASS')
