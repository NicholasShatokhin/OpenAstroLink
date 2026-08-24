#!/usr/bin/env python3
import json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
cpp=(root/'drivers/gemini/oal_driver_gemini.cpp').read_text(encoding='utf-8')
manifest=json.loads((root/'drivers/gemini/oal_driver_gemini.manifest.json').read_text(encoding='utf-8'))
required_cpp=[
    'gOpenSettleMs{2200}',
    'gResetRecoveryMs{1200}',
    'remainingRecoveryMs = gResetRecoveryMs',
    'no reply after %2 ms quiet-open settle',
    'identityOk = populateIdentity(result, &probeReply)',
    'resetRecoveryMs',
]
for token in required_cpp:
    assert token in cpp, f'missing Gemini reset-recovery token: {token}'
assert manifest['config']['openSettleMs'] == 2200
assert manifest['config']['resetRecoveryMs'] == 1200
print(f'gemini_reset_recovery_check: PASS ({len(required_cpp)+2} assertions)')
