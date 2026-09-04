from pathlib import Path
root=Path(__file__).resolve().parents[1]
checks=0

def need(path,*tokens):
    global checks
    s=(root/path).read_text(encoding='utf-8')
    for t in tokens:
        checks+=1
        if t not in s: raise SystemExit(f'FAIL {path}: missing {t!r}')

need('CMakeLists.txt','VERSION 0.2.10.51')
need('src/algorithms/autofocus_engine.cpp',
     'meterSceneExposure','starting focus position %1 restored','probes both','best.position == center.position',
     'Scene focus peak was not bracketed','sceneFocusScore','255.0 / 65535.0','cancelAndRestore')
need('src/core/remote_observatory_controller.cpp','fixed physical sensor scale','255.0/65535.0')
need('src/gui/main_window.h','histogramAutoConverged_','histogramAutoBestExposure_','blockStartMode_','blockStartAt_','blockParkAfter_')
need('src/gui/main_window.cpp','hiPct-lo<8||lo>=248','AUTO LOCKED near optimum','Histogram auto exposure LOCKED','Use current telescope pointing (J2000)',
     'Use current mount pointing as target','Update selected','Delete selected','Clear calendar','Move up','Move down','At date/time')
need('src/core/astro_types.h','QDateTime startAtUtc','QDateTime scheduledStartUtc','startAtUtc')
need('src/algorithms/scheduler.cpp','"scheduled"','"waiting-block-start"','"waiting-camera"','beginScheduled')
need('src/core/application_controller.cpp','Scheduler preflight: stopping Live View operation','waiting-camera','armScheduledSessionStart','camera.live-view')
need('docs/openapi.yaml','version: 0.2.10.51','startAtUtc:')
need('docs/SCHEDULER.md','v0.2.10.49','waiting-camera')
print(f'PASS v0.2.10.51 AF/exposure/scheduler lifecycle: {checks} assertions')
