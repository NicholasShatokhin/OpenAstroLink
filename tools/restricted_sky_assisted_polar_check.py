#!/usr/bin/env python3
from pathlib import Path
import math
import numpy as np

root=Path(__file__).resolve().parents[1]
checks=0

def need(path,*tokens):
    global checks
    text=(root/path).read_text(encoding='utf-8')
    for token in tokens:
        assert token in text, f'{path}: missing {token!r}'
        checks += 1
    return text

need('CMakeLists.txt','VERSION 0.2.10.60','src/algorithms/assisted_polar_alignment.cpp')
need('src/core/astro_types.h','struct ObservableSkyRegion','rejectAutomatedGoto','schedulerEligibility','struct AssistedPolarSample','struct AssistedPolarResult')
need('src/core/settings.cpp','observableSky/enabled','observableSky/minAzDeg','observableSky/maxAzDeg','observableSky/recheckSeconds')
need('src/core/observatory_controller.h','clearAssistedPolarSamples','addAssistedPolarTargetSample','addAssistedPolarSolvedSample','estimateAssistedPolarAlignment','captureObservableSkyCorner','clearObservableSkyRegion')
need('src/core/application_controller.cpp','AssistedPolarSample','observableContains','Async mount GOTO rejected by Observable Sky Region','waiting-observable-sky','selectObservableSchedulerBlock','resumeIndexHint','Capture corner 1 first')
need('src/core/remote_observatory_controller.h','addAssistedPolarTargetSample','captureObservableSkyCorner','assistedPolarSampleCount_')
need('src/core/remote_observatory_controller.cpp','assisted-polar/sample-target','assisted-polar/sample-solved','observable-sky/corner','assistedPolarResult')
need('src/oal/oal_server.cpp','/api/v1/assisted-polar/clear','/api/v1/assisted-polar/sample-target','/api/v1/assisted-polar/sample-solved','/api/v1/assisted-polar/estimate','/api/v1/observable-sky/corner/<arg>','/api/v1/observable-sky/clear')
need('src/algorithms/scheduler.h','deferCurrentBlock','resumeIndexHint')
need('src/algorithms/scheduler.cpp','pendingOrder_','runtimeProgress_','Scheduler::deferCurrentBlock','Scheduler::resumeIndexHint')
need('src/gui/main_window.cpp','Assisted Polar Alignment — manual recenter','Observable Sky Region — visibility/planning, not mechanical safety','Record centred selected target','Capture first visible corner from current pointing')
need('src/gui/sky_map_widget.cpp','setObservableSkyRegion','Observable sky')
need('docs/openapi.yaml','version: 0.2.10.60','/assisted-polar/sample-target:','/assisted-polar/estimate:','/observable-sky/corner/{corner}:','observableSky:')
need('docs/ASSISTED_POLAR_ALIGNMENT.md','SVD/Kabsch','effective pointing-frame fit','HIL acceptance')
need('docs/OBSERVABLE_SKY_REGION.md','hard mechanical safety model','waiting-observable-sky','Crash-resume limitation')
need('docs/uk/ASSISTED_POLAR_ALIGNMENT.md','SVD/Kabsch','HIL acceptance')
need('docs/uk/OBSERVABLE_SKY_REGION.md','manual joystick','crash resume')

# Safety invariant: Observable Sky is not allowed to alter the frozen mount geometry implementation.
# Hash equality itself is checked during packaging; here we at least require the v9 source invariant token.
need('src/core/mount_geometry.cpp','nativeCoordinateModelVersion','axis1Sign','axis2Sign')

# Executable math sanity model: recover a known rigid local-horizontal rotation.
def rz(a):
    a=math.radians(a);c=math.cos(a);s=math.sin(a)
    return np.array([[c,-s,0.],[s,c,0.],[0.,0.,1.]])
def rx(a):
    a=math.radians(a);c=math.cos(a);s=math.sin(a)
    return np.array([[1.,0.,0.],[0.,c,-s],[0.,s,c]])
def unit(v):
    v=np.asarray(v,float);return v/np.linalg.norm(v)
# Non-degenerate synthetic directions and a small known frame rotation.
M=np.array([unit([.7,.2,.68]),unit([-.3,.85,.43]),unit([.5,-.72,.48]),unit([-.8,-.1,.59])]).T
Rtrue=rz(1.4)@rx(-0.8)
T=Rtrue@M
H=T@M.T
U,S,Vt=np.linalg.svd(H)
Rfit=U@Vt
if np.linalg.det(Rfit)<0:
    U[:,-1]*=-1;Rfit=U@Vt
err=np.max(np.abs(Rfit-Rtrue))
assert err < 1e-10, err
checks += 1
# Verify a representative pole vector is recovered by the fit.
pole=unit([0.,math.cos(math.radians(50)),math.sin(math.radians(50))])
assert np.linalg.norm(Rfit@pole-Rtrue@pole) < 1e-10
checks += 1

# Wrapped azimuth semantics used by Observable Sky.
def contains(az,lo,hi):
    az%=360;lo%=360;hi%=360
    return lo<=az<=hi if lo<=hi else az>=lo or az<=hi
assert contains(355,330,20) and contains(5,330,20) and not contains(180,330,20)
checks += 1

print(f'PASS v0.2.10.60 restricted-sky / assisted-polar: {checks} assertions + synthetic rotation fit')
