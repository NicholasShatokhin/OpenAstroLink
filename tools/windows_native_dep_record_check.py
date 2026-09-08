from pathlib import Path

root = Path(__file__).resolve().parents[1]
cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
build = (root / "scripts" / "build_windows.ps1").read_text(encoding="utf-8")
bootstrap = (root / "scripts" / "bootstrap_native_dependencies.ps1").read_text(encoding="utf-8")

required_cmake = [
    '.oal/native-deps-windows-x64.json',
    'string(JSON _oas_record_prefix',
    'CMAKE_PREFIX_PATH',
    'LibDataChannel_DIR',
    'OAL native dependency record:',
    'OAL native WebRTC package:',
]
for token in required_cmake:
    assert token in cmake, f"missing CMake native dependency import token: {token}"

assert "LibDataChannel_DIR = $libDataChannelDir" in bootstrap
assert "'CMAKE_PREFIX_PATH','OpenCV_DIR','LibDataChannel_DIR'" in build
assert 'if(WIN32 AND NOT CMAKE_CROSSCOMPILING)' in cmake
assert 'MATCHES "-NOTFOUND$"' in cmake
print("WINDOWS_NATIVE_DEP_RECORD_CHECK_PASS")
