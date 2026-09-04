#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parents[1]
checks=0
def need(path,*tokens):
    global checks
    text=(root/path).read_text(encoding='utf-8')
    for token in tokens:
        assert token in text, f'{path}: missing {token!r}'
        checks+=1
need('CMakeLists.txt','VERSION 0.2.10.50')
need('src/core/astro_types.h','ObservationMode { DsoFits, PlanetarySer, MosaicFits }','QDateTime startAtUtc;','bool parkAfter{false};','struct MosaicFitsBlock','overlapPercent','currentTileIndex')
need('src/algorithms/scheduler.cpp','waiting-block-start','block->startAtUtc','prepareCurrentBlockStart','MosaicFits')
need('src/core/settings.cpp','scheduler/planJson','scheduler/armed','scheduler/nextBlockIndex','polarMotion/minAzDeg')
need('src/core/application_controller.cpp','mosaicTargetFor','tangentOffsetEquatorial','completeCurrentObservationBlock','park-after-block','horizontalAllowed','if(!limits.enabled)','Polar-alignment slew rejected by safe sky region','safe-region constraint %4')
need('src/gui/main_window.cpp','Add mosaic','Save calendar on node','Arm / start calendar','Park mount after this block','Optional Polar-alignment safe sky region','Restrict Polar Alignment motion to this safe sky region','Works with or without a safe-region constraint','Use current telescope pointing (J2000)')
need('docs/openapi.yaml','mosaic-fits','parkAfter:','startAtUtc:','MosaicFitsBlock:')
print(f'PASS v0.2.10.50 calendar/mosaic/polar safety: {checks} assertions')

app=(root/'src/core/application_controller.cpp').read_text()
assert 'Automatic polar alignment requires an enabled Polar safe sky region' not in app, 'must not require safe region'
