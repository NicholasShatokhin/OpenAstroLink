#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
cmake = (root / 'CMakeLists.txt').read_text(encoding='utf-8')
astro = (root / 'src/core/astro_types.h').read_text(encoding='utf-8')
app = (root / 'src/core/application_controller.cpp').read_text(encoding='utf-8')
main_h = (root / 'src/gui/main_window.h').read_text(encoding='utf-8')
main_cpp = (root / 'src/gui/main_window.cpp').read_text(encoding='utf-8')
sky_h = (root / 'src/gui/sky_map_widget.h').read_text(encoding='utf-8')
sky_cpp = (root / 'src/gui/sky_map_widget.cpp').read_text(encoding='utf-8')
stell_h = (root / 'src/gui/stellarium_remote_control_client.h').read_text(encoding='utf-8')
stell_cpp = (root / 'src/gui/stellarium_remote_control_client.cpp').read_text(encoding='utf-8')

assert 'VERSION 0.2.10.59' in cmake
assert 'src/gui/sky_map_widget.cpp' in cmake
assert 'src/gui/stellarium_remote_control_client.cpp' in cmake
assert 'struct SkyFrame' in astro and 'skyFrameToJson' in astro and 'skyFrameFromJson' in astro
assert 'lastSolvedFrame' in app and 'scaleArcsecPerPx*double(frame.image.cols)/3600.0' in app
assert 'buildSkyMapPanel' in main_h and 'buildSkyMapPanel' in main_cpp
assert 'leftTabs_->addTab(buildSkyMapPanel(),"Sky Map")' in main_cpp
for token in ['Slew','Sync','Abort slew','Park','Unpark','Use in Scheduler']:
    assert token in main_cpp, token
for token in ['equatorialToHorizontal','horizontalToEquatorial','unprojectHorizontal','setCustomSelectionAtPoint','customSelection_','double-click','wheel to zoom','Telescope','Solved','Target','M31 Andromeda Galaxy','M42 Orion Nebula']:
    assert token in sky_cpp or token in sky_h, token
for token in ['setSolvedFrame','setMainPlannedFrame','setGuidePlannedFrame','setPlannerMosaic','plannerEnvelopeFrame','frameCorners','drawSkyFrame']:
    assert token in sky_h or token in sky_cpp, token
for token in ['Solved','Main planned','Guide planned','Mosaic planner grid','Main PA, deg','Guide PA, deg','Stellarium frame export','Send frame','Mosaic envelope']:
    assert token in main_cpp, token
assert 'SpecialMarkersMgr' in stell_h or 'SpecialMarkersMgr' in stell_cpp
for token in ['api/stelproperty/list','api/stelproperty/set','api/main/view','api/main/fov','fovRectangularMarkerWidth','fovRectangularMarkerHeight','fovRectangularMarkerRotationAngle']:
    assert token in stell_cpp, token
assert 'c_->slewMount' in main_cpp
assert 'c_->syncMount' in main_cpp
assert 'c_->abortMountMotion' in main_cpp
assert 'c_->parkMount' in main_cpp
assert 'targetRa_->setValue' in main_cpp and 'targetDec_->setValue' in main_cpp
assert 'click any visible sky position' in main_cpp
assert 'Sky point RA %1h Dec %2%3°' in sky_cpp
assert (root / 'docs/SKY_MAP.md').is_file()
assert (root / 'docs/uk/SKY_MAP.md').is_file()
assert 'id="skymap"' in (root / 'site/index.html').read_text(encoding='utf-8')
assert 'id="skymap"' in (root / 'site/uk/index.html').read_text(encoding='utf-8')
print('Sky Map framing UI smoke: PASS')
