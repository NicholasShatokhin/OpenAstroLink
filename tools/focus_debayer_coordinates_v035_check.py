#!/usr/bin/env python3
from pathlib import Path
import shutil, subprocess
root=Path(__file__).resolve().parents[1]
read=lambda p:(root/p).read_text(encoding='utf-8')
cmake=read('CMakeLists.txt'); astro=read('src/core/astro_types.h'); app=read('src/core/application_controller.cpp'); remote=read('src/core/remote_observatory_controller.cpp'); server=read('src/oal/oal_server.cpp'); gui=read('src/gui/main_window.cpp'); gh=read('src/gui/main_window.h'); af=read('src/algorithms/autofocus_engine.cpp'); eqh=read('src/core/equatorial_frames.h'); eqc=read('src/core/equatorial_frames.cpp'); qhy=read('drivers/qhy/oal_driver_qhy.cpp'); zwo=read('drivers/zwo_asi/oal_driver_zwo_asi.cpp'); native=read('src/backends/oal_native_devices.cpp'); openapi=read('docs/openapi.yaml')
checks={
 'release version': 'VERSION 0.2.10.50' in cmake,
 'generic Bayer enum': all(x in astro for x in ('BayerPattern','RGGB','BGGR','GRBG','GBRG')),
 'optional debayer request': 'bool debayer{false}' in astro and 'bayerPattern{BayerPattern::Auto}' in astro,
 'remote debayer transport': '"debayer",r.debayer' in remote and '"bayerPattern",bayerPatternName' in remote,
 'HTTP debayer transport': 'q.debayer=b.value("debayer")' in server and 'q.bayerPattern=bayerPatternFromString' in server,
 'preview only UI': 'Debayer color preview (preview only; science data stays RAW)' in gui,
 'manual patterns UI': all(x in gui for x in ('"RGGB"','"BGGR"','"GRBG"','"GBRG"')),
 'debayer forces 1x1': 'Live View debayer enabled: forcing 1x1 readout' in app,
 'RGB passthrough': 'camera already supplies RGB' in app,
 'OpenCV verified CFA mapping': all(x in app for x in ('RGGB:return cv::COLOR_BayerBG2BGR','BGGR:return cv::COLOR_BayerRG2BGR','GRBG:return cv::COLOR_BayerGB2BGR','GBRG:return cv::COLOR_BayerGR2BGR')),
 'QHY SDK CFA metadata': 'IsQHYCCDControlAvailable(handle,CAM_COLOR)' in qhy and 'case 1:return "GBRG"' in qhy and 'knownQhyBayerPattern' not in qhy,
 'QHY raw kept raw': 'SetQHYCCDDebayerOnOff(c.handle,false)' in qhy or 'SetQHYCCDDebayerOnOff(c->handle,false)' in qhy or 'SetQHYCCDDebayerOnOff(c.handle, false)' in qhy,
 'ZWO CFA metadata': 'i.BayerPattern' in zwo and 'asiBayerName' in zwo,
 'ZWO native live': all(x in zwo for x in ('camera.liveStart','ASIStartVideoCapture','camera.liveFrame','ASIGetVideoData','camera.liveStop','ASIStopVideoCapture')),
 'native frame Bayer metadata': 'frame.bayerPattern' in native and 'frame.bayerEncoded' in native,
 '16-bit color preview conversion': 'i.depth()==CV_16U)i.convertTo(bgr8,CV_8U,255.0/65535.0)' in app and 'i.depth()==CV_16U)i.convertTo(bgr8,CV_8U,255.0/65535.0)' in remote,
 'saturation quality only': 'Valid camera frame — exposure-quality warning' in gui and 'NOT a camera or transport error' in gui,
 'AF operational preview': 'publishOperationalPreview(f,"af")' in app and '%1-preview-%2' in app,
 'manual focus jog': 'Manual focus jog' in gui and 'Manual focuser STOP' in gui and 'focusJogStep_' in gh,
 'scene native-scale metric': 'sceneFocusScore' in af and 'gray.convertTo(f, CV_32F, 1.0 / 65535.0)' in af,
 'scene flat curve guard': 'Scene focus peak was not bracketed' in af and 'starting focus position %1 restored' in af,
 'scene fine cannot replace stronger coarse': 'candidate->score > chosen.score * 1.005' in af and 'candidate->score>best->score*0.25' not in af,
 'horizontal conversion API': all(x in eqh for x in ('HorizontalCoord','equatorialToHorizontal','horizontalToEquatorial','localSiderealTimeDeg')),
 'galactic conversion API': all(x in eqh for x in ('GalacticCoord','equatorialToGalactic','galacticToEquatorial')),
 'coordinate transforms implemented': all(x in eqc for x in ('equatorialToHorizontal','horizontalToEquatorial','equatorialToGalactic','galacticToEquatorial')),
 'synchronized mount fields': 'Target coordinates — synchronized' in gui and 'synchronizeMountCoordinatesFrom' in gui and all(x in gh for x in ('mountJNowRa_','mountAz_','mountGalL_')),
 'Polaris preset': '37.9545607' in gui and '89.2641090' in gui,
 'canonical GUI GOTO J2000': 'EquatorialCoord t{mountRa_->value(),mountDec_->value(),EquatorialFrame::J2000}' in gui,
 'OpenAPI debayer': 'bayerPattern:' in openapi and 'Preview-only software debayer' in openapi,
}
failed=[]
for k,v in checks.items():
 print(('PASS' if v else 'FAIL')+': '+k)
 if not v: failed.append(k)
if failed: raise SystemExit(f'{len(failed)} v0.2.10.45 assertion(s) failed')
# Keep the QHY API-shape compile independent of the full Qt build.
cxx=shutil.which('g++') or shutil.which('clang++')
if cxx:
 cmd=[cxx,'-std=c++20','-Wall','-Wextra','-Wpedantic','-Werror','-fsyntax-only',f'-I{root/"tests/stubs/qhyccd"}',f'-I{root/"include"}',str(root/'drivers/qhy/oal_driver_qhy.cpp')]
 subprocess.run(cmd,check=True);print('PASS: QHY SDK API-shape compile')
print(f'focus/debayer/coordinates v0.2.10.45: PASS ({len(checks)} assertions)')
