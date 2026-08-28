#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parents[1]
checks=[]
def want(path, text, label):
    data=(root/path).read_text(encoding='utf-8', errors='ignore')
    checks.append((label,text in data))
def reject(path,text,label):
    data=(root/path).read_text(encoding='utf-8', errors='ignore')
    checks.append((label,text not in data))
want(Path('src/node/main.cpp'),'automatic periodic probing is disabled','node disables periodic native rediscovery')
reject(Path('src/node/main.cpp'),'reconnectTimer','legacy 10-second reconnect timer removed')
want(Path('src/oal/oal_server.cpp'),'refreshNativeDiscoveryAsync({},true)','refresh API starts explicit async discovery')
want(Path('src/core/equatorial_frames.cpp'),'2306.2181','J2000/JNow precession implementation present')
want(Path('src/core/astro_types.h'),'EquatorialFrame { J2000, JNow }','coordinate frame type present')
want(Path('src/gui/main_window.cpp'),'J2000 / catalog','GUI exposes J2000 selector')
want(Path('tools/ascom_host/main.cpp'),'EquatorialSystem','Classic ASCOM reports native equatorial system')
want(Path('docs/openapi.yaml'),'coordinateFrame','OAL API documents coordinate frame')
failed=[label for label,ok in checks if not ok]
for label,ok in checks: print(('PASS' if ok else 'FAIL'),label)
if failed: raise SystemExit(1)
print(f'{len(checks)}/{len(checks)} PASS')
