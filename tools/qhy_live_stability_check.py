#!/usr/bin/env python3
from pathlib import Path
import shutil, subprocess

root = Path(__file__).resolve().parents[1]
qhy = (root/'drivers/qhy/oal_driver_qhy.cpp').read_text(encoding='utf-8')
app = (root/'src/core/application_controller.cpp').read_text(encoding='utf-8')
gui = (root/'src/gui/main_window.cpp').read_text(encoding='utf-8')
backend_h = (root/'src/backends/oal_native_devices.h').read_text(encoding='utf-8')
backend_cpp = (root/'src/backends/oal_native_devices.cpp').read_text(encoding='utf-8')
remote = (root/'src/core/remote_observatory_controller.cpp').read_text(encoding='utf-8')
server = (root/'src/oal/oal_server.cpp').read_text(encoding='utf-8')
openapi = (root/'docs/openapi.yaml').read_text(encoding='utf-8')

checks = {
    'native QHY streaming advertised': 'qhyccd-live' in qhy,
    'stream mode selected before InitQHYCCD': qhy.find('SetQHYCCDStreamMode(c.handle,streamMode)') < qhy.find('InitQHYCCD(c.handle)'),
    'native live begin': 'BeginQHYCCDLive(c->handle)' in qhy,
    'native live frame': 'GetQHYCCDLiveFrame(c->handle' in qhy,
    'native live stop': 'StopQHYCCDLive(c->handle)' in qhy,
    'single-frame restore after live': 'reopenCameraMode(*c,0,err)' in qhy and 'single-frame mode restored' in qhy,
    'live cancellation avoids single-frame abort': 'if(c->liveActive)return true' in qhy,
    'health is nonblocking': 'std::try_to_lock' in qhy,
    'health skips live stream': '||c->liveActive' in qhy,
    'health requires consecutive failures': 'healthFailures<3' in qhy,
    'health avoids chip-info presence probe': 'GetQHYCCDChipInfo(c->handle,&chipW' not in qhy[qhy.find('const char *health'):qhy.find('bool setOptional')],
    'controller skips health while camera resource locked': 'operations_.isResourceLocked(resource,&owner)' in app,
    'controller uses native live path': 'native->startNativeLive' in app and 'native->nextNativeLiveFrame' in app and 'native->stopNativeLive' in app,
    'QHY live stop not routed through abortExposure': '!camera_->backendName().startsWith("native:oal.qhy/")' in app,
    'NativeOalCamera live API exists': all(x in backend_h for x in ('nativeLiveSupported','startNativeLive','nextNativeLiveFrame','stopNativeLive')),
    'native frame decoding reused for live': 'decodeNativeCameraFrame' in backend_cpp and 'allowScienceSave' in backend_cpp,
    'daylight safe defaults': 'liveExposure_=dspin(0.0001,10.0,0.001,4)' in gui and 'liveGain_=ispin(0,102400,0)' in gui,
    'white-frame guidance is quality-only': 'Valid camera frame — exposure-quality warning' in gui and 'NOT a camera or transport error' in gui,
    'remote capture forwards saveRaw': '{"saveRaw",r.saveRaw}' in remote,
    'remote capture forwards savePath': 'q["savePath"]=r.savePath' in remote,
    'HTTP capture restores science request': 'q.saveRaw=b.value("saveRaw").toBool(false)' in server and 'q.savePath=b.value("savePath").toString()' in server,
    'OpenAPI documents science preservation': 'saveRaw:' in openapi and 'savePath:' in openapi,
}

failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(('PASS' if ok else 'FAIL') + ': ' + name)
if failed:
    raise SystemExit(f'{len(failed)} QHY Live stability assertion(s) failed')

cxx = shutil.which('g++') or shutil.which('clang++')
if cxx:
    cmd = [cxx, '-std=c++20', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-fsyntax-only',
           f'-I{root / "tests/stubs/qhyccd"}', f'-I{root / "include"}', str(root/'drivers/qhy/oal_driver_qhy.cpp')]
    subprocess.run(cmd, check=True)
    print('PASS: QHYCCD API-shape compile')
else:
    print('SKIP: QHYCCD API-shape compile (no C++ compiler)')

print(f'QHY Live stability v0.2.10.35: PASS ({len(checks)} assertions)')
