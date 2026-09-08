#!/usr/bin/env python3
from pathlib import Path
root = Path(__file__).resolve().parents[1]
s = (root/'CMakeLists.txt').read_text(encoding='utf-8')
checks = {
    'webrtc package prefix derived': 'get_filename_component(_oas_webrtc_prefix' in s,
    'release runtime bin': 'set(_oas_webrtc_runtime_dir "${_oas_webrtc_prefix}/bin")' in s,
    'runtime dll glob': '_oas_webrtc_runtime_candidates' in s and '/*.dll' in s,
    'build-tree copy': 'file(COPY ${_oas_webrtc_runtime_dlls} DESTINATION "${CMAKE_BINARY_DIR}")' in s,
    'install staging': 'install(FILES ${_oas_webrtc_runtime_dlls} DESTINATION bin)' in s,
    'CRT filtering': 'oal_is_microsoft_crt_dll("${_oas_dll}" _oas_is_crt)' in s,
    'required runtime failure': 'WebRTC is required but its Windows runtime directory was not found' in s,
}
for k,v in checks.items():
    print(('PASS' if v else 'FAIL') + ': ' + k)
if not all(checks.values()):
    raise SystemExit(1)
print(f'WINDOWS_WEBRTC_RUNTIME_STAGING_CHECK_PASS {sum(checks.values())}/{len(checks)}')
