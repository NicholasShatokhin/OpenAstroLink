from pathlib import Path
root=Path(__file__).resolve().parents[1]
checks=[]
def need(path,*tokens):
    s=(root/path).read_text(encoding='utf-8')
    for t in tokens:
        assert t in s, f'{path}: missing {t!r}'
        checks.append((path,t))
need('CMakeLists.txt','VERSION 0.2.10.48','src/algorithms/planet_detector.cpp')
need('src/algorithms/planet_detector.h','struct PlanetDetection','class PlanetDetector')
need('src/core/astro_types.h','struct PlanetaryTrackingPolicy','allowRoiShift','mountCorrections','calibrationArcsec','struct PlanetarySerBlock','everyNRuns','cv::Rect roi{}')
need('src/core/application_controller.cpp','startCurrentPlanetarySlew','startCurrentPlanetaryAcquire','startCurrentPlanetaryAutofocus','planetary.mount-calibration','camera.planetary-ser','Planet was not detected','PLANET_LOST','tracker-shift','mount-correction','planetaryMountCalibration_')
need('src/core/ser_writer.cpp','.roi.jsonl','appendRoiEvent','ROIProvenanceFile','InitialROIX')
need('drivers/qhy/oal_driver_qhy.cpp','SetQHYCCDResolution','"roi"')
need('drivers/zwo_asi/oal_driver_zwo_asi.cpp','ASISetROIFormat','ASISetStartPos','"roi"')
need('drivers/qhy/oal_driver_qhy.manifest.json','0.2.10.47')
need('drivers/zwo_asi/oal_driver_zwo_asi.manifest.json','0.2.10.47')
need('src/gui/main_window.cpp','Add planetary SER block','Track target by moving hardware ROI','Allow calibrated mount corrections','mixed-mode executor')
need('docs/openapi.yaml','version: 0.2.10.48','PlanetarySerBlock:','mountCorrections:','roiShiftThresholdPx:')
need('docs/PLANETARY_SER.md','v0.2.10.47','.roi.jsonl','hardware ROI','defaults OFF')
need('docs/uk/PLANETARY_SER.md','v0.2.10.47','.roi.jsonl','hardware ROI')
print(f'scheduler planetary SER executor v0.2.10.47: PASS ({len(checks)} assertions)')
