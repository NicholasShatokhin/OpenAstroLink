#!/usr/bin/env python3
from pathlib import Path
import sys

root=Path(__file__).resolve().parents[1]
checks=[]
def has(path, needle, label):
    text=(root/path).read_text(encoding='utf-8',errors='replace')
    ok=needle in text
    checks.append((ok,label))

def lacks(path, needle, label):
    text=(root/path).read_text(encoding='utf-8',errors='replace')
    ok=needle not in text
    checks.append((ok,label))

has('CMakeLists.txt','VERSION 0.2.10.58','current release version')
has('docs/openapi.yaml','captureFpsLimit:','OpenAPI high-rate capture limit')
has('docs/openapi.yaml','previewFpsLimit:','OpenAPI independent preview limit')
has('docs/HIGH_RATE_STREAMING.md','OALV v1','streaming architecture documentation')
has('docs/RELEASE_0.2.10.58.md','WebRTC','release note')
has('src/core/astro_types.h','double captureFpsLimit{0.0};','capture rate independently unlimited by default')
has('src/core/astro_types.h','double previewFpsLimit{60.0};','preview defaults to 60 FPS')
has('src/core/astro_types.h','int bitsPerSample{8};','high-rate 8-bit live default')
has('src/core/astro_types.h','int previewMaxWidth{1280};','preview downscale independent from acquisition')
has('src/core/observatory_controller.h','startGuideLiveView','guide Live View API')
has('src/core/application_controller.cpp','camera.guide.live-view','independent guide-camera resource operation')
has('src/core/application_controller.cpp','QThreadPool::globalInstance()->start','preview processing off acquisition thread')
has('src/core/application_controller.cpp','previewBusy->exchange(true)','droppable latest preview work')
has('src/core/application_controller.cpp','appendSer(frame,serError)','raw SER write in acquisition path')
has('src/core/application_controller.cpp','"recordDropped",0','record drop telemetry')
has('src/oal/oal_ws_server.cpp','out.append("OALV",4)','binary OALV video framing')
has('src/oal/oal_ws_server.cpp','path=="/video"','dedicated /video WebSocket path')
has('src/oal/oal_ws_server.cpp','eventClients_','events isolated from video clients')
has('src/oal/oal_ws_server.cpp','QThreadPool::globalInstance()->start','JPEG encoding off event-loop thread')
has('src/oal/oal_ws_server.cpp','sendBinaryMessage(packet)','binary WebSocket video delivery')
has('src/core/remote_observatory_controller.cpp','setPath("/events")','dedicated remote event WebSocket')
has('src/core/remote_observatory_controller.cpp','setPath("/video")','dedicated remote video WebSocket')
has('src/core/remote_observatory_controller.cpp','binaryMessageReceived','remote binary video receiver')
has('src/core/remote_observatory_controller.cpp','onWsBinary','OALV binary decoder')
has('src/oal/oal_server.cpp','cameraRole!="main"&&cameraRole!="guide"','main/guide live REST roles')
has('src/gui/main_window.cpp','"Dual Live"','dual camera alignment view')
has('src/gui/main_window.cpp','"8-bit (high-rate)"','live bit-depth control')
has('src/gui/main_window.cpp','"Preview max width (0=full)"','preview bandwidth control')
has('drivers/qhy/oal_driver_qhy.cpp','std::vector<unsigned char> liveBuffer;','QHY reusable live buffer')
has('drivers/qhy/oal_driver_qhy.cpp','SetQHYCCDBitsMode(c->handle,requestedBits)','QHY high-rate transfer-bit selection')
has('drivers/zwo_asi/oal_driver_zwo_asi.cpp','std::vector<unsigned char> liveBuffer;','ASI reusable live buffer')
has('drivers/zwo_asi/oal_driver_zwo_asi.cpp','preferredImageType(c->info,requestedBits)','ASI live bit-depth selection')
has('src/backends/oal_native_devices.cpp','frame.nativeBacking=std::move(native.bytes);','native frame avoids second full-frame clone')
has('src/core/application_controller.cpp','elapsed.elapsed()-lastAnalysisMs>=50','planetary tracking analysis decimated from raw record rate')
has('src/core/application_controller.cpp','oalv1-jpeg-planetary','planetary preview uses droppable OALV path')
lacks('src/core/application_controller.cpp','std::clamp(r.targetFps,0.2,30.0)','old 30 FPS clamp removed')
lacks('drivers/qhy/oal_driver_qhy.cpp','\\"maxFps\\":30','stale QHY 30 FPS capability removed')
has('src/core/application_controller.cpp','publishedStats["sourceWidth"]','OALV preserves source preview width separately')
has('src/core/application_controller.cpp','publishedStats["width"]=image.width()','OALV width describes encoded JPEG payload')
has('src/core/remote_observatory_controller.cpp','QImage rgb=image.convertToFormat(QImage::Format_RGB888)','remote OALV JPEG decoded once')
lacks('src/core/remote_observatory_controller.cpp','std::vector<uchar> bytes(jpeg.begin(),jpeg.end())','remote OALV avoids second JPEG decode')
has('src/core/application_controller.cpp','The same native camera cannot be assigned to both Main and Guide','same native camera cannot occupy both Dual Live roles')
has('drivers/qhy/oal_driver_qhy.cpp','if(c->liveBuffer.empty()){const auto length=GetQHYCCDMemLength','QHY per-frame mem-length query removed from steady state')

failed=[label for ok,label in checks if not ok]
for ok,label in checks:
    print(('PASS' if ok else 'FAIL')+': '+label)
print(f'High-rate / Dual Live: {len(checks)-len(failed)}/{len(checks)} checks passed')
if failed:
    sys.exit(1)
