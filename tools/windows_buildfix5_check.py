#!/usr/bin/env python3
from pathlib import Path
import re
root = Path(__file__).resolve().parents[1]
ws = (root/'src/oal/oal_ws_server.cpp').read_text(encoding='utf-8')
cm = (root/'CMakeLists.txt').read_text(encoding='utf-8')
ps = (root/'scripts/bootstrap_vendor_sdks.ps1').read_text(encoding='utf-8')
assert 'std::min(kWebRtcFragmentPayload,packet.size()-offset)' not in ws
assert 'const int remaining=int(packet.size())-offset;' in ws
assert 'std::min(kWebRtcFragmentPayload,remaining)' in ws
assert 'EAF_focuser-static.lib' in ps
assert "-notmatch '(?i)(^|[-_])static([-. _]|$)'" in ps
assert 'NAMES EAF_focuser EAFFocuser EAFFocuser.bin' in cm
assert 'ZWO EAF: replaced static archive with DLL import library' in cm
assert 'EAF_focuser.lib EAFFocuser.lib' in cm
assert 'ZWO_EAF_RUNTIME_DIR' in cm and 'paired with import library' in cm
print('WINDOWS_BUILDFIX5_CHECK_PASS')
