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
need(Path('src/core/astro_types.h'),'apertureMm','guideFocalLengthMm','guideArcsecPerPixel','focalRatio()')
need(Path('src/core/settings.cpp'),'guideCameraBinding','guide/apertureMm','profile/apertureMm')
need(Path('src/core/observatory_controller.h'),'connectGuideCamera','startGuideCapture','lastGuideFrame')
need(Path('src/core/application_controller.cpp'),'camera.guide','guideCamera_','role","guide','startGuideCapture')
need(Path('src/oal/oal_server.cpp'),'role=="guide"','startGuideCapture','guide-camera')
need(Path('src/gui/main_window.cpp'),'Guide camera','Guide optical train','guideCameraBackend_')
need(Path('docs/OPTICAL_TRAINS_AND_DUAL_CAMERAS.md'),'camera.guide','role','aperture')
if errs:
    print('Dual-camera / optical-profile check: FAIL'); [print(' -',x) for x in errs]; sys.exit(1)
print('Dual-camera / optical-profile check: PASS')
