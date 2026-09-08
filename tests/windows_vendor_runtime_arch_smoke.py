from pathlib import Path
root = Path(__file__).resolve().parents[1]
cm = (root/'CMakeLists.txt').read_text(encoding='utf-8')
diag = (root/'scripts/diagnose_windows_runtime.ps1').read_text(encoding='utf-8')
repair = (root/'scripts/repair_windows_vendor_runtime.ps1').read_text(encoding='utf-8')
bootstrap = (root/'scripts/bootstrap_vendor_sdks.ps1').read_text(encoding='utf-8') + (root/'scripts/bootstrap_native_dependencies.ps1').read_text(encoding='utf-8')
checks = {
    'PE parser exists': 'function(oal_windows_pe_machine' in cm,
    'AMD64 machine id': '6486' in cm and 'AMD64' in cm,
    'I386 rejected path': '4c01' in cm and 'I386' in cm,
    'stale build-root deletion': 'file(REMOVE "${CMAKE_BINARY_DIR}/${_oal_runtime_name}")' in cm,
    'stale driver-dir deletion': 'file(REMOVE "${OAL_DRIVER_BUILD_DIR}/${_oal_runtime_name}")' in cm,
    'wrong arch warning': 'refusing wrong-architecture runtime DLL' in cm,
    'diagnostic BinaryReader': 'System.IO.BinaryReader' in diag,
    'diagnostic error 193': 'error 193' in diag,
    'strict ABI pass token': 'WINDOWS_RUNTIME_ABI_CHECK_PASS' in diag,
    'repair command exists': 'WINDOWS_VENDOR_RUNTIME_REPAIR_PASS' in repair,
    'repair stages AMD64 only': "Get-PeMachine $f.FullName) -eq 'AMD64'" in repair,
    'bootstrap validates AMD64 DLL': "Get-PeMachine $_.FullName) -eq 'AMD64'" in bootstrap,
}
for k,v in checks.items():
    print(f"{'PASS' if v else 'FAIL'}: {k}")
assert all(checks.values())
print(f"WINDOWS_VENDOR_RUNTIME_ARCH_SMOKE_PASS {sum(checks.values())}/{len(checks)}")
