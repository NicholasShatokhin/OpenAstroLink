#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
cmake = (root / 'CMakeLists.txt').read_text(encoding='utf-8')
bootstrap = (root / 'scripts' / 'bootstrap_native_dependencies.ps1').read_text(encoding='utf-8')

checks = {
    'cmake_pair_comment': 'Treat the\n        # import library and runtime DLL directory as one ABI/version pair.' in cmake,
    'cmake_expected_machine_helper_not_recursive': 'function(oal_expected_windows_pe_machine out_machine)\n    oal_expected_windows_pe_machine(' not in cmake,
    'cmake_stage_uses_expected_machine_helper': 'oal_expected_windows_pe_machine(_oal_expected_machine)' in cmake,
    'cmake_arch_root_from_import_lib': 'get_filename_component(_oas_canon_arch_root "${_oas_canon_import_dir}" DIRECTORY)' in cmake,
    'cmake_sibling_runtime': 'set(_oas_canon_paired_runtime "${_oas_canon_arch_root}/${_oas_canon_dll_leaf}")' in cmake,
    'cmake_primary_edsdk_dll': 'set(_oas_canon_primary_dll "${_oas_canon_paired_runtime}/EDSDK.dll")' in cmake,
    'cmake_pe_machine_validation': 'oal_windows_pe_machine("${_oas_canon_primary_dll}"' in cmake,
    'cmake_force_pair_cache': 'paired with CANON_EDSDK_LIBRARY' in cmake and 'CACHE PATH' in cmake and 'FORCE)' in cmake,
    'bootstrap_pair_arch_root': "$archRoot = Split-Path -Parent $l.Directory.FullName" in bootstrap,
    'bootstrap_pair_primary_dll': "Join-Path (Join-Path $archRoot $leaf) 'EDSDK.dll'" in bootstrap,
    'bootstrap_amd64_gate': "(Get-PeMachine $paired) -eq 'AMD64'" in bootstrap,
}
failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(f"{'PASS' if ok else 'FAIL'} {name}")
if failed:
    raise SystemExit(f"CANON_EDSDK_PAIRING_CHECK_FAIL: {', '.join(failed)}")
print(f"CANON_EDSDK_PAIRING_CHECK_PASS {len(checks)}/{len(checks)}")
