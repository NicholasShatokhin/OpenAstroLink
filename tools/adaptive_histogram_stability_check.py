from pathlib import Path
root=Path(__file__).resolve().parents[1]
checks=0

def need(path,*tokens):
    global checks
    text=(root/path).read_text(encoding='utf-8')
    for token in tokens:
        checks+=1
        if token not in text:
            raise SystemExit(f'FAIL {path}: missing {token}')

need('CMakeLists.txt','VERSION 0.2.10.35')
need('src/core/astro_types.h','maxCapturePhaseSec{120.0}')
need('src/core/application_controller.cpp','softwareBinForSolver','Adaptive solver software bin','ADAPTIVE_SOLVE_CAPTURE_BUDGET_EXCEEDED','saturationFraction>0.01','background>0.45','capturePhaseTimer','state!="running"||o.value("phase").toString()=="starting"')
need('src/core/remote_observatory_controller.cpp','maxCapturePhaseSec')
need('src/oal/oal_server.cpp','maxCapturePhaseSec')
need('src/gui/main_window.cpp','Preview histogram / exposure assistant','Compute after each received frame','Auto-apply suggested exposure to next capture','P1=%1%','ISO/gain unchanged','updateHistogram')
need('src/gui/main_window.h','histogramEnabled_','histogramSuggestedExposure_','updateHistogram')
need('docs/openapi.yaml','version: 0.2.10.35','maxCapturePhaseSec')
print(f'PASS adaptive/histogram stability guard: {checks} assertions')
