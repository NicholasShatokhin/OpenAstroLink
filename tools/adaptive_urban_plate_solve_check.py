from pathlib import Path
import sys
root=Path(__file__).resolve().parents[1]
checks=0

def need(path,*tokens):
    global checks
    text=(root/path).read_text(encoding='utf-8')
    for t in tokens:
        checks+=1
        if t not in text:
            raise SystemExit(f'FAIL {path}: missing {t}')

need(Path('CMakeLists.txt'),'VERSION 0.2.10.49','adaptive_plate_solve.cpp','oal-adaptive-plate-solve-smoke')
need(Path('src/core/astro_types.h'),'AdaptiveSolveRequest','binX{1}','binY{1}')
need(Path('src/algorithms/adaptive_plate_solve.cpp'),'equalized16','motion_.estimate','registeredFrames','GaussianBlur','warpAffine')
need(Path('src/algorithms/astap_solver.cpp'),'frame.binY','bin changes the angular size')
need(Path('src/core/application_controller.cpp'),'solver.adaptive','solve.capture','solve.quality','solve.astap','useMountHint','ADAPTIVE_SOLVE_FAILED')
need(Path('src/core/observatory_controller.h'),'startAdaptiveSolve')
need(Path('src/core/remote_observatory_controller.cpp'),'solve/adaptive','startAdaptiveSolve')
need(Path('src/oal/oal_server.cpp'),'"/api/v1/solve/adaptive"','maxSingleExposureSec','equalizeBackground')
need(Path('src/gui/main_window.cpp'),'Adaptive urban capture + solve','solveBaseExposure_','r.maxSingleExposureSec=solveMaxExposure_->value()','std::min(solveBaseExposure_->value(),r.maxSingleExposureSec)','solveStackFrames_','setAdaptiveSolveBusy')
need(Path('docs/PLATE_SOLVING.md'),'register frames on stars','mount RA/Dec','solverFrameId')
need(Path('docs/uk/PLATE_SOLVING.md'),'міського','solverFrameId')
need(Path('docs/openapi.yaml'),'/solve/adaptive:','version: 0.2.10.49')
need(Path('src/backends/opencv_camera.cpp'),'cv::INTER_AREA','f.binX=binX','f.binY=binY')
need(Path('src/backends/simulated_devices.cpp'),'cv::INTER_AREA','frame.binX = binX','frame.binY = binY')
need(Path('src/backends/oal_native_devices.cpp'),'actualBinX','expectedW','cannot silently corrupt plate scale')
need(Path('tests/adaptive_plate_solve_smoke.cpp'),'syntheticUrbanField','registeredFrames < 3','detectedStars < 10')
print(f'PASS adaptive urban plate solve foundation: {checks} assertions')
