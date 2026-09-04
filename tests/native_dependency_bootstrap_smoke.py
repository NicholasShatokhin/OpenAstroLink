from pathlib import Path
import json
root = Path(__file__).resolve().parents[1]
required = [
    'scripts/bootstrap_qt_native.sh',
    'scripts/bootstrap_qt_native.ps1',
    'scripts/bootstrap_native_dependencies.sh',
    'scripts/bootstrap_native_dependencies.ps1',
    'scripts/discover_canon_edsdk.sh',
]
for f in required:
    assert (root/f).exists(), f

qt = (root/'scripts/bootstrap_qt_native.sh').read_text()
for token in ['aqtinstall', 'qthttpserver', 'qtwebsockets', 'qtserialport', 'qtpositioning', '6.8.3', 'linux_gcc_64', 'list-qt']:
    assert token in qt, token
assert 'aqt_arch="gcc_64"' not in qt
assert '--modules "${required_modules[@]}"' in qt
native = (root/'scripts/bootstrap_native_dependencies.sh').read_text()
assert 'apt-get update unconditionally' in native
assert 'DPkg::Lock::Timeout=120' in native
linux = (root/'scripts/build_linux.sh').read_text()
assert 'auto_deps=1' in linux
assert 'bootstrap_native_dependencies.sh' in linux
assert 'CMAKE_PREFIX_PATH' in linux and 'OpenCV_DIR' in linux
mac = (root/'scripts/build_macos.sh').read_text()
assert 'bootstrap_native_dependencies.sh' in mac and 'auto_deps=1' in mac
win = (root/'scripts/build_windows.ps1').read_text()
assert 'bootstrap_native_dependencies.ps1' in win and 'NoAutoDeps' in win
win_boot = (root/'scripts/bootstrap_native_dependencies.ps1').read_text()
assert 'vcpkg' in win_boot and 'OpenCV' in win_boot
win_vendor = (root/'scripts/bootstrap_vendor_sdks.ps1').read_text()
assert 'DeveloperCameraSdk' in win_vendor and 'DeveloperEafSdk' in win_vendor
canon = (root/'scripts/discover_canon_edsdk.sh').read_text()
assert 'never downloaded' in canon or 'does not download' in canon
cmake = (root/'CMakeLists.txt').read_text()
for token in ['OAS_QT_ROOT', 'OAS_VENDOR_SDK_ROOT', 'OAL native Qt prefix', '/qhy', '/zwo/asi', '/zwo/eaf']:
    assert token in cmake, token
example = json.loads((root/'CMakeUserPresets.example.json').read_text())
linux_p = next(x for x in example['configurePresets'] if x['name']=='my-linux-observatory')
assert 'CMAKE_PREFIX_PATH' not in linux_p['cacheVariables']
assert 'OAS_QT_ROOT' not in linux_p['cacheVariables']
assert linux_p['cacheVariables']['CANON_EDSDK_ROOT'] == '${sourceDir}/../edsdk'
assert linux_p['cacheVariables']['OAS_ENABLE_INDI'] == 'OFF'
print('native dependency auto-bootstrap smoke: PASS')
