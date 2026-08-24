#!/usr/bin/env python3
import json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
cpp=(root/'drivers/gemini/oal_driver_gemini.cpp').read_text(encoding='utf-8')
manifest=json.loads((root/'drivers/gemini/oal_driver_gemini.manifest.json').read_text(encoding='utf-8'))
cfg=manifest['config']
assert cfg['openSettleMs'] == 2200, cfg
assert cfg['resetRecoveryMs'] == 1200, cfg
assert 'gOpenSettleMs{2200}' in cpp
assert 'gResetRecoveryMs{1200}' in cpp
assert 'serial config: probeTimeoutMs=%1 commandTimeoutMs=%2 openSettleMs=%3 resetRecoveryMs=%4' in cpp
assert 'waiting an additional %3 ms before retry' in cpp
assert 'gResetRecoveryMs - int(sinceOpen.elapsed())' not in cpp
print('gemini_manifest_timing_check: PASS (manifest/runtime defaults synchronized)')
