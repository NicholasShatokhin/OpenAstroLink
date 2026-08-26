from pathlib import Path
import json, sys
root=Path(__file__).resolve().parents[1]
checks=0

def need(path,*tokens):
    global checks
    text=(root/path).read_text(encoding='utf-8')
    for t in tokens:
        checks+=1
        if t not in text:
            raise SystemExit(f'FAIL {path}: missing {t}')

need(Path('CMakeLists.txt'),
     'VERSION 0.2.10.13','OAS_CANON_TRANSPORT','CANON_EDSDK_INCLUDE_DIR','CANON_EDSDK_LIBRARY',
     'oal_driver_canon_edsdk.cpp','QHYCCD_RUNTIME_DIR','ZWO_ASI_RUNTIME_DIR','ZWO_EAF_RUNTIME_DIR',
     'build_features.json')
need(Path('drivers/canon/oal_driver_canon_edsdk.cpp'),
     'EdsInitializeSDK','EdsGetCameraList','EdsOpenSession','EdsDownload','EdsDownloadThumbnail',
     'kEdsCameraCommand_BulbStart','host.publishFrame','0.2.10.13')
need(Path('CMakeUserPresets.example.json'),
     'my-windows-observatory','my-windows-observatory-edsdk','my-linux-observatory','CANON_EDSDK_RUNTIME_DIR','QHYCCD_RUNTIME_DIR')
need(Path('scripts/package_windows.ps1'),'windeployqt','VendorRuntimeDirs','cmake --install')
need(Path('scripts/package_linux.sh'),'cmake --install','RUNTIME_DEPENDENCIES.txt')
need(Path('docs/BUILD_PLATFORMS.md'),'Windows x64','Linux x86_64','Linux ARM64','CMakeUserPresets.json','OAS_CANON_TRANSPORT')
need(Path('docs/uk/BUILD_PLATFORMS.md'),'Windows x64','Linux x86_64','Linux ARM64','CMakeUserPresets.json')

presets=json.loads((root/'CMakePresets.json').read_text())
names={p['name'] for p in presets['configurePresets']}
for n in ['windows-core-release','windows-native-release','windows-observatory-release','linux-native-release','linux-observatory-release','linux-node-release','rpi4-native-release','rpi4-observatory-release','node-sim-release']:
    checks+=1
    if n not in names: raise SystemExit(f'FAIL preset missing {n}')

if 'CMakeUserPresets.json' not in (root/'.gitignore').read_text(): raise SystemExit('FAIL CMakeUserPresets not ignored')
checks+=1
print(f'PASS cross-platform build/deployment foundation: {checks} assertions')
