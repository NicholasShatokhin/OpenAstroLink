from pathlib import Path
root=Path(__file__).resolve().parents[1]
checks=0

def need(path,*tokens):
    global checks
    text=(root/path).read_text(encoding='utf-8')
    for token in tokens:
        checks+=1
        if token not in text:
            raise SystemExit(f'FAIL {path}: missing {token!r}')

need('CMakeLists.txt','VERSION 0.2.10.55')
need('src/backends/synscan_network_mount.cpp',
     'dg.senderPort()!=port_',
     'instantStopAxis',
     'confirmedStopAxis',
     'running&&!gotoMode',
     'desiredRate1=a1==0?0:rate',
     'confirmedStopAxis(1,650',
     'confirmedStopAxis(2,650')
need('src/gui/main_window.cpp',
     'Invert Axis 1 mapping / tracking direction',
     'Invert Axis 2 mapping',
     'Mount target fields copied from Sky Map',
     'brightestRegion(image,&valid,&contrast)',
     'bright-tail P99.5',
     'histogramAutoLastGain_',
     'highClip<=0.0050',
     'Histogram auto exposure LOCKED')
need('src/core/remote_observatory_controller.cpp',
     'if(!frameId.isEmpty()&&frameId!=lastFrame_.id&&!frameId.startsWith("live-"))',
     'binaryMessageReceived',
     'm.left(4)!="OALV"',
     'emit videoFrameCaptured(image,role,stats)')
need('src/algorithms/autofocus_engine.cpp',
     'lv.p995',
     'std::clamp(effective.exposureSec, 0.00005, 2.0)',
     'tileScores',
     'sceneReq.framesPerPosition = std::clamp',
     '0.020 * scale',
     'candidate peak was not repeatable',
     'starting focus %1 is already within the repeatable local maximum')
need('docs/RELEASE_0.2.10.54.md','~0.64 s','~1.43 s','v9 geometry is frozen and unchanged')

# Regression model for the measured sparse-bright sequence.  The new control
# target should NOT reproduce the old 0.64 -> ~1.43 hard-threshold jump.
def next_exposure(current, p995, clip, target=0.62):
    import math
    measured=max(1/255,p995)
    rel=abs(measured-target)/target
    healthy=clip<=0.005 and p995<=0.96
    if rel<=0.12 and healthy:
        return current, True
    factor=(max(0.10,min(10.0,target/measured)))**0.45
    factor=max(0.65,min(1.60,factor))
    if p995>0.88:
        factor=min(factor,max(0.68,min(0.99,(0.88/p995)**0.55)))
    if clip>0.005:
        factor=min(factor,max(0.60,min(0.94,(0.005/clip)**0.35)))
    return current*factor, False

# 0.6381 s frame from HIL: P99.5 ~= 55.4%, clip ~= 0.349%; this is already
# inside the new sparse-scene acquisition band and must lock rather than jump.
nxt,locked=next_exposure(0.6381,0.554,0.00349)
assert locked and abs(nxt-0.6381)<1e-9, (nxt,locked)
print(f'PASS v0.2.10.55 2026-09-06 HIL + binary-live regression: {checks} static assertions + sparse exposure lock model')
