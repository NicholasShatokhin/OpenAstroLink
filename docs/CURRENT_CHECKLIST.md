# OpenAstroLink v0.2.10.58-buildfix9 — current master checklist

**Snapshot:** 2026-09-08  
**Canonical language:** English. Ukrainian mirror: `uk/CURRENT_CHECKLIST.md`.  
**Meaning of states:** ✅ implemented/qualified; 🟡 implemented but current HIL/runtime regression is still required; ⬜ not complete; 🔒 frozen by HIL evidence.

This checklist is the operational release-status view for the current repository. Historical release notes remain useful for provenance, but this file, `../CURRENT_STATUS.md`, `STATUS.md`, `VALIDATION.md`, and `ROADMAP_P0_P1_IMPLEMENTATION.md` define the current gates.

## Core / architecture

| Capability | State |
|---|---|
| Node owns hardware | ✅ |
| Remote/local GUI | ✅ |
| HTTP API | ✅ |
| JSON WebSocket events `/events` | ✅ |
| Binary OALV `/video` preview transport | ✅ |
| WebRTC signaling `/webrtc` | ✅ build-qualified |
| WebRTC DataChannel preview transport | ✅ build-qualified |
| Independent Main/Guide WebRTC channels | ✅ implementation |
| Automatic OALV/WebSocket fallback | ✅ implementation |
| Per-role Main/Guide fallback | ✅ implementation |
| Async operations | ✅ |
| Resource locking | ✅ |
| Independent Main/Guide camera locks | ✅ |
| Cancel operations | ✅ |
| Persisted device bindings | ✅ |
| Native driver ABI/registry | ✅ |
| Native OAL drivers default | ✅ |
| INDI compatibility optional/OFF by default | ✅ |
| English canonical docs + Ukrainian mirror | ✅ |
| Durable recovery inside an operation after crash | ⬜ OAL 1.0 |
| Persistent idempotency/event replay | ⬜ OAL 1.0 |
| TLS/auth/RBAC/audit | ⬜ OAL 1.0 |
| Process-isolated driver host | ⬜ OAL 1.0 |

### WebRTC

Current path is `camera → recording branch → preview worker → JPEG/OALV → WebRTC DataChannel`. Scientific SER/raw recording remains upstream of preview/network work.

| WebRTC item | State |
|---|---|
| libdatachannel 0.24.5 | ✅ |
| Windows dependency bootstrap | ✅ |
| Windows runtime DLL staging | ✅ |
| PeerConnection | ✅ |
| `oalv-main` DataChannel | ✅ |
| `oalv-guide` DataChannel | ✅ |
| Unordered delivery | ✅ |
| Partial reliability | ✅ 150 ms |
| OALW fragmentation/reassembly | ✅ |
| Latest-frame replacement | ✅ |
| 2 MiB backpressure/drop gate | ✅ |
| STUN/TURN configuration | ✅ |
| `/video` fallback | ✅ |
| Node starts with WebRTC runtime DLLs | ✅ runtime smoke |
| OpenCV/UVC Live View | ✅ runtime smoke |
| Confirm actual `webrtc-datachannel` transport, not fallback | 🟡 HIL |
| WebRTC disconnect/fallback/reconnect | 🟡 HIL |
| Simultaneous Main + Guide WebRTC | 🟡 HIL |
| H.264/H.265/AV1 RTP media track | ⬜ optimization |

## Mount

Direct Motor Controller coordinate model **v9 is frozen**: `Axis1Sign=+1`, `Axis2Sign=-1`. Do not change geometry, Home/Park convention, axis meaning, polarity, or serial/Wi-Fi parity without new contradictory HIL evidence.

| Capability | State |
|---|---|
| ASCOM Classic / EQMOD | ✅ HIL |
| Native EQDrive | ✅ HIL |
| Direct SynScan/EQDrive Wi-Fi basic motion | ✅ HIL |
| Backend geometry parity | ✅ |
| Free-point Sky Map GOTO | ✅ HIL |
| GOTO | ✅ |
| Home/Park | ✅ HIL |
| Tracking | ✅ |
| Manual control | ✅ |
| Sync | ✅ |
| GOTO abort | ✅ HIL |
| Stellarium bridge | ✅ |
| J2000/JNow | ✅ |
| GEM/pier geometry | ✅ 🔒 v9 HIL |
| Hidden 15° driver GOTO limit | ✅ removed |
| Operator sky-safety separate from geometry | ✅ |
| Raw-axis mechanical guard | ✅ |
| Stateful axis inversion checkboxes | 🟡 repeat HIL |
| Wi-Fi retry + instant stop | ✅ implementation |
| Manual start/stop post-condition verification | ✅ implementation |
| Fail-safe stop | ✅ implementation |
| 30–50 rapid Wi-Fi press/release/direction changes | 🟡 critical HIL |
| Sky Map target → Scheduler | ✅ HIL |
| Sky Map target → Mount tab | 🟡 repeat HIL |
| Meridian flip automation | ⬜ OAL 1.0 |
| Collision model | ⬜ OAL 1.0 |
| Direct-MC pulse guiding | ⬜ |

The native mount stack is considered geometrically functional. Current mount-side work is robustness requalification, not another geometry rewrite.

## Camera / QHY

| Capability | State |
|---|---|
| QHY discovery/connect | ✅ historical HIL |
| FITS | ✅ HIL |
| Native Live View | ✅ HIL |
| Still capture after Live View | ✅ HIL |
| Gain/offset/binning | ✅ |
| 8-bit high-rate mode | ✅ |
| 16-bit mode | ✅ |
| Bayer/raw | ✅ |
| Hardware ROI | ✅ implementation |
| SER | ✅ HIL |
| SER opens in AutoStakkert | ✅ HIL |
| SER `.txt` metadata | ✅ HIL |
| Raw SER before preview processing | ✅ |
| Artificial 30 FPS cap removed | ✅ |
| `captureFpsLimit=0` = camera maximum | ✅ |
| Preview 15/30/60/MAX | ✅ implementation |
| Preview droppable / recording non-droppable architecture | ✅ |
| Capture/record/preview FPS telemetry | ✅ implementation |
| `previewDropped` telemetry | ✅ |
| `recordDropped` telemetry | ✅ |
| OALV binary preview | ✅ |
| WebRTC preview | ✅ build-qualified |
| QHY MAX-FPS WebRTC HIL | 🟡 |
| 5–10 minute SER endurance | 🟡 |
| `recordDropped=0` endurance acceptance | 🟡 |
| AutoStakkert frame-count equality | 🟡 |
| Preview FPS independence from recording | 🟡 |
| QHY hotplug/replug | 🟡 |
| Simultaneous Main + Guide | 🟡 |

High-rate acceptance: short exposure, `Capture FPS=MAX`, `Preview=60`, record `captureFps`, `recordFps`, `previewFps`, `previewDropped`, `recordDropped`. Preview may drop; SER recording must not silently drop.

## Dual Live / two cameras

| Capability | State |
|---|---|
| Main camera independent lock | ✅ |
| Guide camera independent lock | ✅ |
| Main + Guide simultaneous operations | ✅ implementation |
| Independent exposure/gain | ✅ |
| Independent capture/preview limits | ✅ |
| Different physical camera requirement | ✅ |
| Duplicate native camera protection | ✅ |
| Two OALV/WebRTC streams | ✅ |
| Dual Live GUI | ✅ |
| Real Main+Guide HIL | 🟡 |
| Per-role partial WebRTC failure | 🟡 |
| Optical-axis alignment workflow | 🟡 HIL |

## ZWO

| Component | State |
|---|---|
| ZWO ASI native driver | ✅ |
| ASI hardware ROI | ✅ |
| SER/live path | ✅ |
| ZWO EAF native driver | ✅ |
| Windows EAF DLL import-library selection | ✅ buildfix5+ |
| Static-archive misuse guard | ✅ |
| Runtime DLL PE architecture validation | ✅ |
| ASI real HIL | ⬜ |
| EAF real HIL | 🟡 |
| `oal.zwo.eaf` node-registry runtime load after buildfix9 | 🟡 immediate |

## Canon EOS

| Capability | State |
|---|---|
| Native EDSDK backend | ✅ |
| EOS 550D historical real HIL | ✅ |
| CR2/original transfer | ✅ historical HIL |
| Normal exposure | ✅ |
| Bulb foundation | ✅ |
| Canon x64 import library | ✅ |
| Canon runtime PE architecture validation | ✅ |
| `EDSDK_64/Library` ↔ `EDSDK_64/Dll` pair locking | ✅ buildfix9 |
| Automatic x86 EDSDK rejection | ✅ |
| Fresh Windows configure/build with corrected pair | ✅ 2026-09-08 |
| `oal.canon` node-registry runtime load after buildfix9 | 🟡 immediate |
| reconnect/hotplug regression | 🟡 |
| exposure >30 s regression | 🟡 |
| Dedicated EDSDK EVF high-rate Live View | ⬜ |

## Focusing

| Capability | State |
|---|---|
| Gemini native driver | ✅ |
| Hardware movement | ✅ HIL |
| Manual focusing | ✅ |
| AF cancel | ✅ HIL |
| Restore after cancel | ✅ HIL |
| Astronomical/star AF engine | ✅ |
| Scene AF | ✅ implementation |
| Bright-tail AF exposure metering | ✅ |
| Tiled local contrast | ✅ |
| Compact coarse/fine search | ✅ |
| Rollback on flat curve/failure/cancel | ✅ |
| Repeatability gate | ✅ |
| Scheduler AF calls | ✅ |
| Nearly focused scene → AF must not degrade | 🟡 HIL |
| Deliberately defocused → recover focus | 🟡 HIL |
| Scene AF repeatability | 🟡 |
| Night star/HFR AF | 🟡 |
| Temperature coefficient calibration | ⬜ |
| Temperature compensation between exposures | ⬜ |
| Thermal compensation during long session | ⬜ OAL 1.0 |
| Focus-correction science metadata | ⬜ |

Scene AF is implemented but not yet HIL-qualified.

## Still auto exposure

| Capability | State |
|---|---|
| Normalized sensor-scale handling | ✅ |
| No remote min/max restretch | ✅ |
| Sparse-scene bright-tail metering | ✅ |
| P99.5 controller | ✅ |
| Clipping awareness | ✅ |
| Hysteresis | ✅ |
| Crossing logic | ✅ |
| Deadband/LOCK | ✅ |
| Reset on gain change | ✅ |
| Live/AF isolation from still state | ✅ implementation |
| Repeat old ~0.64 ↔ ~1.43 s sparse target | 🟡 |
| Stable convergence | 🟡 HIL |
| LOCK without limit cycle | 🟡 |
| Relock after real scene change | 🟡 |

## Scheduler

### Observation model

| Capability | State |
|---|---|
| `ObservationPlan` | ✅ |
| `ObservationBlock` | ✅ |
| `dso-fits` | ✅ |
| `planetary-ser` | ✅ |
| `mosaic-fits` | ✅ |
| Mixed block types | ✅ |

### Calendar / persistence

| Capability | State |
|---|---|
| Per-task date/time | ✅ |
| Start now / future start | ✅ |
| Months/year planning | ✅ |
| Calendar stored on node | ✅ |
| Persistent completed-block cursor | ✅ |
| Completed blocks not replayed after restart | ✅ |
| Optional Park after | ✅ |
| Auto-unpark before next task | ✅ |
| Resume inside unfinished block | ⬜ OAL 1.0 |

### GUI

| Capability | State |
|---|---|
| Add/Edit/Delete/Clear | ✅ |
| Move up/down | ✅ |
| Use current telescope coordinates | ✅ |
| Per-task date/time | ✅ |
| Park after | ✅ |
| Sky Map target → Scheduler | ✅ HIL |
| Calendar/timeline view | ⬜ nice-to-have |
| Overlap/conflict warning | ⬜ |
| Drag-and-drop | ⬜ non-blocker |

### DSO automation

| Stage | State |
|---|---|
| Slew | ✅ |
| Solve | ✅ |
| Sync/Recenter | ✅ framework |
| Autofocus | ✅ |
| FITS × N | ✅ |
| Autofocus every N | ✅ |
| Solve/recenter every N | ✅ |
| Scheduler camera preflight | ✅ |
| Auto-stop interactive Live View | ✅ |
| Full scheduler DSO HIL | 🟡 |
| ASTAP closed-loop real sky | 🟡 |
| Guiding | ⬜ |
| Dither | ⬜ |
| Filter changes | ⬜ |
| Meridian-flip recovery | ⬜ |

## Planetary automation

Current framework: **GOTO → acquisition → planet detect → center → autofocus → ROI → SER**.

| Capability | State |
|---|---|
| Planetary block | ✅ |
| Full-frame acquisition | ✅ |
| Planet detection | ✅ |
| Centering foundation | ✅ |
| Autofocus stage | ✅ |
| Hardware ROI | ✅ |
| Multiple SER runs | ✅ |
| Pauses | ✅ |
| Fast ROI tracking | ✅ |
| `.roi.jsonl` provenance | ✅ |
| Planet-lost detection | ✅ |
| Optional slow mount correction | ✅ implementation |
| Camera↔mount response calibration foundation | ✅ |
| Real planetary scheduler HIL | 🟡 |
| Automatic physical centering | 🟡 |
| ROI follows target | 🟡 |
| Slow mount correction HIL | 🟡 |
| Calibration-frame ROI reuse | ⬜ |
| Long-session ephemeris refresh | ⬜ |

## Mosaic capture

| Capability | State |
|---|---|
| `mosaic-fits` block | ✅ |
| Rows × columns | ✅ |
| Overlap % | ✅ |
| Camera rotation | ✅ |
| FOV from sensor + optics | ✅ |
| Tangent-plane tile calculation | ✅ |
| Serpentine traversal | ✅ |
| Slew per tile | ✅ |
| Solve/recenter per tile | ✅ |
| Optional AF per tile | ✅ |
| FITS × N per tile | ✅ |
| Camera footprints | ✅ implementation |
| Sky Map mosaic overlay | ✅ implementation |
| Stellarium FOV export | ✅ implementation |
| Real 2×2 mosaic HIL | 🟡 |
| Overlay end-to-end HIL | 🟡 |
| Mosaic manifest | 🟡 implementation |
| Resume individual missing tiles | ⬜ OAL 1.0 |

## Polar Alignment

| Capability | State |
|---|---|
| Manual sample workflow | ✅ |
| Automatic multi-solve workflow | ✅ |
| Controlled RA slews | ✅ |
| Polar-axis error estimate | ✅ |
| Optional safe-sky region | ✅ |
| Wrapped azimuth region | ✅ |
| Full slew-path validation | ✅ |
| Safe region OFF by default | ✅ |
| Real-sky HIL | 🟡 |
| Accuracy test | 🟡 |
| Guided Alt/Az bolt instructions | 🟡 |
| Iterative convergence | 🟡 |
| Final verification solve | 🟡 |

## Astrometry

| Capability | State |
|---|---|
| ASTAP integration | ✅ |
| Solve operation | ✅ |
| Scheduler solve | ✅ |
| Sync/recenter framework | ✅ |
| Real-sky ASTAP qualification | 🟡 |
| ~1–2 arcmin acceptance | 🟡 |
| Retries foundation | ✅ |
| Failure recovery | 🟡 |
| Alternative solver fallback | ⬜ |

## Guiding

| Capability | State |
|---|---|
| Guide-camera role | ✅ |
| Simultaneous Guide Live View | ✅ implementation |
| Calibration | ⬜ |
| Guide-star detection | ⬜ |
| RA/DEC correction loop | ⬜ |
| Direct pulse guide | ⬜ |
| Dithering | ⬜ |
| Settle | ⬜ |
| Lost-star recovery | ⬜ |
| Scheduler integration | ⬜ |

Guiding is not a hard blocker for the first supervised Beta, but remains a major production gap.

## Other observatory devices

API/profile foundations exist for Filter Wheel, Rotator, Dome/Roof, Weather, GPS, Power, Cover/Calibrator, Safety Monitor, and Switch. Production real-device drivers/workflows are primarily **OAL 1.0** scope rather than a supervised Beta blocker.

## Discovery / hotplug

| Capability | State |
|---|---|
| Async discovery | ✅ |
| Explicit Refresh | ✅ |
| Persisted bindings | ✅ |
| Gemini COM migration | ✅ HIL |
| Predictable QHY hotplug | 🟡 |
| Canon hotplug | 🟡 |
| Gemini unplug/replug | 🟡 |
| Stale GUI device cleanup | 🟡 |
| One-click Refresh guarantee | 🟡 |
| Automatic reconnect policy | ⬜ |

## Website

| Item | State |
|---|---|
| Site source | ✅ |
| English | ✅ |
| Ukrainian | ✅ |
| Feature sections | ✅ |
| High-rate/WebRTC content | ✅ source |
| Scheduler content | ✅ |
| Supported Hardware matrix | 🟡 |
| HIL Pending / Planned matrix | 🟡 |
| Protocol/API docs | ✅ foundation |
| Roadmap | ✅ |
| Deploy `openastro.link` | ⬜ |
| DNS/HTTPS | ⬜ deployment |
| Downloads/Releases | ⬜ |

## Windows release

| Item | State |
|---|---|
| Qt 6.10 / MSVC2022 x64 | ✅ |
| Current source/core | **v0.2.10.58** |
| Current Windows working-tree checkpoint | **buildfix9** |
| WebRTC/libdatachannel bootstrap | ✅ |
| vcpkg dependency bootstrap | ✅ |
| WebRTC configure | ✅ |
| WebRTC runtime DLL staging | ✅ |
| QHY SDK/runtime staging | ✅ |
| ZWO ASI staging | ✅ |
| ZWO EAF import-library repair | ✅ |
| Canon EDSDK x64 pair locking | ✅ |
| PE architecture validation | ✅ |
| Warning cleanup | ✅ |
| Fresh buildfix9 configure | ✅ 2026-09-08 |
| Fresh MSVC buildfix9 | ✅ 2026-09-08 |
| `Linking CXX executable OpenAstroSuite.exe` | ✅ |
| Optional Vulkan SDK | ⬜ not required |
| OpenCV/UVC node+Live View runtime smoke | ✅ previous `.58` lineage |
| Canon driver runtime load after buildfix9 | 🟡 immediate |
| ZWO EAF runtime load after buildfix9 | 🟡 immediate |
| Portable release package | ⬜ |
| Qt/OpenCV release deployment | 🟡 foundation |
| WebRTC runtime deployment | ✅ build tree |
| Vendor SDK runtimes | ✅ build tree/configure validation |
| Clean-machine install test | ⬜ |
| Installer/portable ZIP | ⬜ |
| Release notes | ✅ development checkpoint |
| Signed checksums | ⬜ release |

Static qualification retained for the `.58` branch: **75/75 general scripts PASS**, **43/43 WebRTC PASS**, **43/43 high-rate/Dual-Live PASS**.

## What currently blocks 0.3 Beta

The old `Clean Windows build` blocker is closed. Current P0 is:

1. 🟡 Canon + ZWO EAF node-registry runtime load regression after buildfix9.
2. 🟡 QHY high-rate HIL: MAX capture / Preview 60.
3. 🟡 5–10 minute SER, `recordDropped=0`, AutoStakkert frame-count verification.
4. 🟡 Preview 15/30/60/MAX without reducing recording FPS.
5. 🟡 Actual WebRTC transport + failure/fallback HIL.
6. 🟡 Main + Guide Dual Live HIL.
7. 🟡 30–50 direct-Wi-Fi manual press/release/direction-change HIL.
8. 🟡 Scene autofocus HIL and repeatability.
9. 🟡 Sparse-scene still auto-exposure convergence/LOCK HIL.
10. 🟡 Scheduler DSO HIL.
11. 🟡 ASTAP solve + recenter HIL.
12. 🟡 Planetary SER/ROI executor HIL.
13. 🟡 Mosaic 2×2 + footprint overlay HIL.
14. 🟡 Polar Alignment real-sky HIL.
15. 🟡 Discovery/hotplug regression.
16. ⬜ Windows portable release + clean-machine test.
17. ⬜ Deploy `openastro.link`.

Mount geometry is no longer a Beta blocker. Windows compilation is no longer a blocker. WebRTC implementation is no longer a blocker; **physical transport/camera qualification is**.

## Beta → OAL 1.0

After the supervised Beta, the major production track remains: durable FITS/SER/block resume; TLS/auth/RBAC; persistent audit/event journal; production guiding+dither; automatic meridian flip; weather; roof/dome; power; emergency shutdown; unattended safety state machine; thermal focus compensation; filter wheel; calibration manager; disk/storage safeguards; resumable downloads; process-isolated drivers; public driver SDK/conformance suite; production RTP video codecs; and Smart Telescope UX.
