#!/usr/bin/env python3
from pathlib import Path
import json, sys
root=Path(__file__).resolve().parents[1]
checks=[]
def need(path,*tokens):
    p=root/path
    if not p.exists(): checks.append((False,f'missing {path}')); return
    text=p.read_text(errors='replace')
    for t in tokens: checks.append((t in text,f'{path}: {t}'))
need(Path('CMakeLists.txt'),'OAS_ENABLE_NATIVE_CANON','oal_driver_canon','libgphoto2','JPEG::JPEG')
need(Path('drivers/canon/oal_driver_canon.cpp'),'oal.canon','gp_camera_autodetect','gp_camera_capture','eosremoterelease','Press Full MF','abortMode','GP_FILE_TYPE_PREVIEW','publishFrame','saveRaw','scienceFilePath','OAL_PIXEL_RGB8','camera.abortExposure')
need(Path('src/backends/oal_native_devices.cpp'),'savePath')
need(Path('src/core/application_controller.cpp'),'migrateCanonDescriptor','oal.canon')
need(Path('src/tools/hardware_probe.cpp'),'Native Canon EOS: OK','require-native-observatory')
need(Path('scripts/bootstrap_rpi_observatory.sh'),'libgphoto2-dev','oal.canon')
need(Path('scripts/install_rpi_node.sh'),'/var/lib/openastrolink/captures/canon')
need(Path('docs/CANON_EOS.md'),'native OpenAstroLink ABI-v2','RAW/CR2/CR3','libgphoto2')
try:
    j=json.loads((root/'drivers/canon/oal_driver_canon.manifest.json').read_text())
    checks += [(j.get('driverId')=='oal.canon','manifest driverId'),(j.get('abiVersion')==2,'manifest ABI v2'),('camera' in j.get('deviceClasses',[]),'manifest camera class')]
except Exception as e: checks.append((False,f'manifest parse: {e}'))
failed=[m for ok,m in checks if not ok]
if failed:
    print('Native Canon EOS driver check: FAIL')
    for m in failed: print(' -',m)
    sys.exit(1)
print(f'Native Canon EOS driver check: PASS ({len(checks)} assertions)')
