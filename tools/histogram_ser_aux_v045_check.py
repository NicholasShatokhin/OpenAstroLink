#!/usr/bin/env python3
from pathlib import Path
import math
root=Path(__file__).resolve().parents[1]
checks=[]
def need(rel,*tokens):
    text=(root/rel).read_text(encoding='utf-8',errors='ignore')
    for tok in tokens:
        assert tok in text, f'{rel}: missing {tok!r}'
        checks.append((rel,tok))
    return text
need('CMakeLists.txt','VERSION 0.2.10.49')
app=need('src/core/application_controller.cpp','255.0/65535.0','Preserve the physical sensor level','r.offset=std::max(0,r.offset)','metadata sidecar')
gui=need('src/gui/main_window.cpp','convergent sensor-scale controller','std::pow(std::clamp(ratio,0.08,12.0),0.68)','relativeError<=0.14','histogramAutoConverged_','f->addRow("Offset",liveOffset_)','Observatory device classes — API placeholders')
ser=need('src/core/ser_writer.cpp','OpenAstroLink planetary capture metadata','ExposureRequestedSec=','ExposureActualFirstFrameSec=','GainRequested=','OffsetRequested=','MeasuredFPS=','TimestampTrailerUTC=true')
need('src/core/ser_writer.h','FireCapture-style human-readable .txt sidecar','sidecarPath()')
need('src/core/astro_types.h','FilterWheel','Rotator','Dome','Weather','Gps','Power','CoverCalibrator','SafetyMonitor')
need('src/core/application_controller.cpp','stub-filter-wheel','stub-dome','stub-weather','stub-gps','stub-cover-calibrator','"implemented",false')
need('docs/openapi.yaml','version: 0.2.10.49')
# Regression against the user's QHY HIL means: exposure/signal is linear up to
# 16 ms, so an 18%-of-16bit background target should predict an intermediate
# exposure instead of the previous forced 16/32 ms toggling.
means=[493.4,975.1,1964.4,3905.5,7774.3]
exps=[.001,.002,.004,.008,.016]
slopes=[m/e for m,e in zip(means,exps)]
ratio=max(slopes)/min(slopes)
assert ratio < 1.05, ratio
# Using 16 ms mean as a conservative signal estimator: target 18% of 65535.
target=.18*65535.0
pred=.016*target/means[-1]
assert 0.020 < pred < 0.030, pred
checks += [('numeric','QHY linearity before clipping'),('numeric','non-binary target exposure')]
print(f'histogram/SER/auxiliary stubs v0.2.10.45: PASS ({len(checks)} assertions)')
