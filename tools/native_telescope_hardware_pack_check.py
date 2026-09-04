#!/usr/bin/env python3
from pathlib import Path
import json, sys
root=Path(__file__).resolve().parents[1]
checks=[]
def need(path,*tokens):
    p=root/path
    if not p.exists(): checks.append((False,f'missing {path}')); return
    text=p.read_text(errors='replace')
    for token in tokens: checks.append((token in text,f'{path}: {token}'))

need(Path('CMakeLists.txt'),'VERSION 0.2.10','OAS_ENABLE_NATIVE_CANON','OAS_ENABLE_NATIVE_GEMINI','OAS_ENABLE_NATIVE_SKYWATCHER','QHYCCD_INCLUDE_DIR','QHYCCD_LIBRARY')
need(Path('drivers/common_blocking_serial_session.h'),'OalBlockingSerialSession','BlockingQueuedConnection','keeps one physical port open','bool dtr = false','setDataTerminalReady(dtr)')
need(Path('drivers/gemini/myfocuserpro2_protocol.h'),':02#',':00#',':01#',':04#',':06#',':08#',':05')
need(Path('drivers/gemini/oal_driver_gemini.cpp'),'oal.gemini','9600','probeCommand()','controller-reported','powerCycleMayInvalidateMechanicalReference','focuser.moveAbsolute','focuser.moveRelative','focuser.halt','NOT_SUPPORTED',':8#','persistentSession')
need(Path('drivers/skywatcher/synscan_protocol.h'),'16777216.0','gotoRaDec','syncRaDec','parsePreciseRaDec','echoProbe')
need(Path('drivers/skywatcher/motor_controller_protocol.h'),'EXPERIMENTAL','instantStop','initDone')
need(Path('drivers/skywatcher/oal_driver_skywatcher.cpp'),'oal.skywatcher','SynScan Serial Communication Protocol 3.3','J2000','MOUNT_NOT_ALIGNED','mount.abort','mount.sync','mount.setTracking','mount.pulseGuide','fixed-rate-1','SynScan serial protocol 3.3 does not define a normative park command','experimental-codec-only','wifiUdpPort", 11880','return ok(QJsonObject{{"accepted", true}')
need(Path('src/tools/hardware_probe.cpp'),'require-native-telescope','require-native-observatory','Native telescope pack: OK','Native observatory pack: OK')
need(Path('scripts/enable_indi_compat.sh'),'indi-bin','OAS_ENABLE_INDI=OFF','OAS_ENABLE_INDI=ON')
need(Path('packaging/systemd/openastrolink-node.env.example'),'OAL_GEMINI_PORT','OAL_SKYWATCHER_PORT')
need(Path('tests/native_protocol_smoke.cpp'),'Native telescope protocol smoke tests passed')

for path,driver,klass in [
    ('drivers/gemini/oal_driver_gemini.manifest.json','oal.gemini','focuser'),
    ('drivers/skywatcher/oal_driver_skywatcher.manifest.json','oal.skywatcher','mount'),
    ('drivers/qhy/oal_driver_qhy.manifest.json','oal.qhy','camera'),
    ('drivers/canon/oal_driver_canon.manifest.json','oal.canon','camera'),
]:
    try:
        j=json.loads((root/path).read_text())
        checks += [(j.get('abiVersion')==2,f'{path}: ABI v2'),
                   (j.get('driverId')==driver,f'{path}: driverId'),
                   (klass in j.get('deviceClasses',[]),f'{path}: device class')]
    except Exception as e: checks.append((False,f'{path}: {e}'))

presets=json.loads((root/'CMakePresets.json').read_text())
by={p['name']:p for p in presets['configurePresets']}
def resolve(n):
    p=by[n]; r={}; inh=p.get('inherits')
    if isinstance(inh,str): r.update(resolve(inh))
    elif isinstance(inh,list):
        for x in inh:r.update(resolve(x))
    r.update(p.get('cacheVariables',{})); return r
checks.append((resolve('rpi4-native-release').get('OAS_ENABLE_INDI')=='OFF','native-only preset disables INDI'))
checks.append((resolve('rpi4-native-release').get('OAS_ENABLE_NATIVE_CANON')=='ON','native-only preset enables native Canon'))
checks.append((resolve('rpi4-observatory-release').get('OAS_ENABLE_INDI')=='OFF','observatory preset keeps INDI opt-in'))
checks.append((resolve('rpi4-observatory-indi-release').get('OAS_ENABLE_INDI')=='ON','explicit observatory INDI preset enables compatibility'))
failed=[m for ok,m in checks if not ok]
if failed:
    print('Native telescope hardware pack check: FAIL')
    for m in failed: print(' -',m)
    sys.exit(1)
print(f'Native telescope hardware pack check: PASS ({len(checks)} assertions)')
