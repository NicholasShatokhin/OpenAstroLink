#!/usr/bin/env python3
import json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
presets=json.loads((root/'CMakePresets.json').read_text())
cn={x['name']:x for x in presets['configurePresets']}
assert cn['rpi4-cross-arm64-observatory-release']['cacheVariables']['OAS_BUILD_GUI']=='ON'
assert cn['rpi4-cross-arm64-observatory-release']['inherits']=='rpi4-cross-arm64-node-release'
assert cn['rpi4-cross-arm64-node-release']['cacheVariables']['OAS_BUILD_GUI']=='OFF'
for f in ['scripts/bootstrap_vendor_sdks.sh','scripts/bootstrap_vendor_sdks.ps1','scripts/fetch_qhy_sdk.sh']:
    assert (root/f).exists(), f
s=(root/'scripts/bootstrap_vendor_sdks.sh').read_text()
assert 'qhyccd.com/file/repository/publish/SDK' in s
assert 'indilib/indi-3rdparty' in s and 'b0802f2' in s
w=(root/'scripts/bootstrap_vendor_sdks.ps1').read_text()
assert 'sdk_win64_' in w and 'QHYCCD' in w
assert 'DeveloperCameraSdk' in w and 'DeveloperEafSdk' in w
print('native vendor/bootstrap + RPi GUI preset smoke: PASS')
