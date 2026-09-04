#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
cmake = (root / 'CMakeLists.txt').read_text(encoding='utf-8')
main_h = (root / 'src/gui/main_window.h').read_text(encoding='utf-8')
main_cpp = (root / 'src/gui/main_window.cpp').read_text(encoding='utf-8')
sky_h = (root / 'src/gui/sky_map_widget.h').read_text(encoding='utf-8')
sky_cpp = (root / 'src/gui/sky_map_widget.cpp').read_text(encoding='utf-8')

assert 'VERSION 0.2.10.51' in cmake
assert 'src/gui/sky_map_widget.cpp' in cmake
assert 'buildSkyMapPanel' in main_h and 'buildSkyMapPanel' in main_cpp
assert 'leftTabs_->addTab(buildSkyMapPanel(),"Sky Map")' in main_cpp
for token in ['Slew','Sync','Abort slew','Park','Unpark','Use in Scheduler']:
    assert token in main_cpp, token
for token in ['equatorialToHorizontal','double-click','wheel to zoom','Telescope','Solved','M31 Andromeda Galaxy','M42 Orion Nebula']:
    assert token in sky_cpp or token in sky_h, token
assert 'c_->slewMount' in main_cpp
assert 'c_->syncMount' in main_cpp
assert 'c_->abortMountMotion' in main_cpp
assert 'c_->parkMount' in main_cpp
assert 'targetRa_->setValue' in main_cpp and 'targetDec_->setValue' in main_cpp
assert (root / 'docs/SKY_MAP.md').is_file()
assert (root / 'docs/uk/SKY_MAP.md').is_file()
assert 'id="skymap"' in (root / 'site/index.html').read_text(encoding='utf-8')
assert 'id="skymap"' in (root / 'site/uk/index.html').read_text(encoding='utf-8')
print('Sky Map UI MVP smoke: PASS')
