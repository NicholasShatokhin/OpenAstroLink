#!/usr/bin/env python3
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[1]; errs=[]
def need(path,*tokens):
    p=root/path
    if not p.exists(): errs.append(f'missing {path}'); return
    s=p.read_text(errors='replace')
    for t in tokens:
        if t not in s: errs.append(f'{path}: missing {t}')
need(Path('src/integrations/stellarium_telescope_server.cpp'),'kGotoPacketSize = 20','kPositionPacketSize = 24','LittleEndian','slewMount','broadcastPosition')
need(Path('src/core/observatory_controller.h'),'startStellariumServer','stellariumPort')
need(Path('src/oal/oal_server.cpp'),'integrations/stellarium','scope","mount-position-and-goto')
need(Path('src/node/main.cpp'),'stellarium-port','startStellariumServer')
need(Path('src/gui/main_window.cpp'),'Stellarium Telescope Control bridge')
need(Path('docs/STELLARIUM.md'),'`10000`','telescope position','GOTO')
if errs:
    print('Stellarium bridge check: FAIL'); [print(' -',x) for x in errs]; sys.exit(1)
print('Stellarium bridge check: PASS')
