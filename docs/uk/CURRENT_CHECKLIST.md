# OpenAstroLink v0.2.10.58-buildfix9 — актуальний master checklist

**Snapshot:** 2026-09-09  
**Canonical:** англійський `../CURRENT_CHECKLIST.md`; цей файл — український mirror.  
**Позначення:** ✅ реалізовано/кваліфіковано; 🟡 реалізовано, але потрібен актуальний HIL/runtime regression; ⬜ не завершено; 🔒 frozen за HIL-доказами.

Цей checklist є оперативним release-status view для поточного repository. Історичні release notes зберігають provenance, але поточні gates визначають цей файл, `../../CURRENT_STATUS_UA.md`, `../STATUS_UA.md`, `VALIDATION.md` і `ROADMAP_P0_P1_IMPLEMENTATION.md`.

## Core / архітектура

| Можливість | Стан |
|---|---|
| Node володіє обладнанням | ✅ |
| Remote/local GUI | ✅ |
| HTTP API | ✅ |
| JSON WebSocket events `/events` | ✅ |
| Binary OALV `/video` preview transport | ✅ |
| WebRTC signaling `/webrtc` | ✅ build-qualified |
| WebRTC DataChannel preview transport | ✅ build-qualified |
| Окремі Main/Guide WebRTC channels | ✅ implementation |
| Automatic OALV/WebSocket fallback | ✅ implementation |
| Per-role Main/Guide fallback | ✅ implementation |
| async operations | ✅ |
| resource locking | ✅ |
| independent Main/Guide camera locks | ✅ |
| cancel operations | ✅ |
| persisted device bindings | ✅ |
| native driver ABI/registry | ✅ |
| Native OAL drivers default | ✅ |
| INDI optional compatibility, OFF by default | ✅ |
| EN canonical docs + UA mirror | ✅ |
| durable recovery всередині operation після crash | ⬜ OAL 1.0 |
| persistent idempotency/event replay | ⬜ OAL 1.0 |
| TLS/auth/RBAC/audit | ⬜ OAL 1.0 |
| process-isolated driver host | ⬜ OAL 1.0 |

### WebRTC

Поточний path: `camera → recording branch → preview worker → JPEG/OALV → WebRTC DataChannel`. Scientific SER/raw recording залишається upstream від preview/network work.

| WebRTC | Стан |
|---|---|
| libdatachannel 0.24.5 | ✅ |
| Windows dependency bootstrap | ✅ |
| Windows runtime DLL staging | ✅ |
| PeerConnection | ✅ |
| `oalv-main` DataChannel | ✅ |
| `oalv-guide` DataChannel | ✅ |
| unordered delivery | ✅ |
| partial reliability | ✅ 150 ms |
| OALW fragmentation/reassembly | ✅ |
| latest-frame replacement | ✅ |
| 2 MiB backpressure/drop gate | ✅ |
| STUN/TURN configuration | ✅ |
| `/video` fallback | ✅ |
| node стартує з WebRTC runtime DLL | ✅ runtime smoke |
| OpenCV/UVC Live View | ✅ runtime smoke |
| підтвердити actual `webrtc-datachannel`, а не fallback | 🟡 HIL |
| WebRTC disconnect/fallback/reconnect | 🟡 HIL |
| Main + Guide simultaneous WebRTC | 🟡 HIL |
| H.264/H.265/AV1 RTP media track | ⬜ optimization |

## Монтування

Direct Motor Controller coordinate model **v9 frozen**: `Axis1Sign=+1`, `Axis2Sign=-1`. Не змінювати geometry, Home/Park convention, axis meaning, polarity або serial/Wi-Fi parity без нових суперечливих HIL-доказів.

| Можливість | Стан |
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
| прихований 15° driver GOTO limit | ✅ removed |
| operator sky-safety окремо від geometry | ✅ |
| raw-axis mechanical guard | ✅ |
| stateful axis inversion checkboxes | 🟡 repeat HIL |
| Wi-Fi retry + instant stop | ✅ implementation |
| start/stop post-condition verification | ✅ implementation |
| fail-safe stop | ✅ implementation |
| 30–50 rapid Wi-Fi press/release/direction changes | 🟡 critical HIL |
| Sky Map target → Scheduler | ✅ HIL |
| Sky Map target → Mount tab | 🟡 repeat HIL |
| Meridian flip automation | ⬜ OAL 1.0 |
| Collision model | ⬜ OAL 1.0 |
| Pulse guiding direct-MC | ⬜ |

Native mount stack вважається геометрично працездатним. Поточна mount-side робота — robustness requalification, а не новий geometry rewrite.

## Камера / QHY

| Можливість | Стан |
|---|---|
| QHY discovery/connect | ✅ historical HIL |
| FITS | ✅ HIL |
| Native Live View | ✅ HIL |
| Capture після Live View | ✅ HIL |
| Gain/offset/binning | ✅ |
| 8-bit high-rate mode | ✅ |
| 16-bit mode | ✅ |
| Bayer/raw | ✅ |
| Hardware ROI | ✅ implementation |
| SER | ✅ HIL |
| SER відкривається в AutoStakkert | ✅ HIL |
| SER `.txt` metadata | ✅ HIL |
| Raw SER before preview processing | ✅ |
| artificial 30 FPS cap removed | ✅ |
| `captureFpsLimit=0` = camera maximum | ✅ |
| Preview 15/30/60/MAX | ✅ implementation |
| preview droppable / recording non-droppable architecture | ✅ |
| capture/record/preview FPS telemetry | ✅ implementation |
| `previewDropped` telemetry | ✅ |
| `recordDropped` telemetry | ✅ |
| OALV binary preview | ✅ |
| WebRTC preview | ✅ build-qualified |
| QHY MAX-FPS WebRTC HIL | 🟡 |
| SER 5–10 хв endurance | 🟡 |
| `recordDropped=0` endurance acceptance | 🟡 |
| AutoStakkert frame-count equality | 🟡 |
| Preview FPS independence from recording | 🟡 |
| QHY hotplug/replug | 🟡 |
| simultaneous Main + Guide | 🟡 |

High-rate acceptance: short exposure, `Capture FPS=MAX`, `Preview=60`; виміряти `captureFps`, `recordFps`, `previewFps`, `previewDropped`, `recordDropped`. Preview може drop-атися; SER recording — ні.

## Dual Live / дві камери

| Можливість | Стан |
|---|---|
| Main camera independent lock | ✅ |
| Guide camera independent lock | ✅ |
| Main + Guide simultaneous operations | ✅ implementation |
| independent exposure/gain | ✅ |
| independent capture/preview limits | ✅ |
| different physical camera requirement | ✅ |
| duplicate native camera protection | ✅ |
| два OALV/WebRTC streams | ✅ |
| Dual Live GUI | ✅ |
| real Main+Guide HIL | 🟡 |
| per-role partial WebRTC failure | 🟡 |
| optical-axis alignment workflow | 🟡 HIL |

## ZWO

| Компонент | Стан |
|---|---|
| ZWO ASI native driver | ✅ |
| ASI hardware ROI | ✅ |
| SER/live path | ✅ |
| ZWO EAF native driver | ✅ |
| Windows EAF DLL import-library selection | ✅ buildfix5+ |
| static-archive misuse guard | ✅ |
| runtime DLL PE architecture validation | ✅ |
| ASI real HIL | ⬜ |
| EAF real HIL | 🟡 |
| `oal.zwo.eaf` node-registry runtime load після buildfix9 | 🟡 immediate |

## Canon EOS

| Можливість | Стан |
|---|---|
| Native EDSDK backend | ✅ |
| EOS 550D historical real HIL | ✅ |
| CR2/original transfer | ✅ historical HIL |
| normal exposure | ✅ |
| Bulb foundation | ✅ |
| Canon x64 import library | ✅ |
| Canon runtime PE architecture validation | ✅ |
| `EDSDK_64/Library` ↔ `EDSDK_64/Dll` pair locking | ✅ buildfix9 |
| automatic x86 EDSDK rejection | ✅ |
| fresh Windows configure/build з corrected pair | ✅ 2026-09-08 |
| `oal.canon` node-registry runtime load після buildfix9 | 🟡 immediate |
| reconnect/hotplug regression | 🟡 |
| exposure >30 s regression | 🟡 |
| dedicated EDSDK EVF high-rate Live View | ⬜ |

## Фокусування

| Можливість | Стан |
|---|---|
| Gemini native driver | ✅ |
| hardware movement | ✅ HIL |
| manual focusing | ✅ |
| AF cancel | ✅ HIL |
| restore after cancel | ✅ HIL |
| astronomical/star AF engine | ✅ |
| Scene AF | ✅ implementation |
| bright-tail AF exposure metering | ✅ |
| tiled local contrast | ✅ |
| compact coarse/fine search | ✅ |
| rollback flat curve/failure/cancel | ✅ |
| repeatability gate | ✅ |
| scheduler AF calls | ✅ |
| nearly-focused scene → AF не погіршує | 🟡 HIL |
| deliberately defocused → recover focus | 🟡 HIL |
| Scene AF repeatability | 🟡 |
| night star/HFR AF | 🟡 |
| temperature coefficient calibration | ⬜ |
| temperature compensation між exposures | ⬜ |
| thermal compensation during long session | ⬜ OAL 1.0 |
| focus-correction science metadata | ⬜ |

Scene AF = implemented, але ще не HIL-qualified.

## Still Auto Exposure

| Можливість | Стан |
|---|---|
| normalized sensor-scale handling | ✅ |
| no remote min/max restretch | ✅ |
| sparse-scene bright-tail metering | ✅ |
| P99.5 controller | ✅ |
| clipping awareness | ✅ |
| hysteresis | ✅ |
| crossing logic | ✅ |
| deadband/LOCK | ✅ |
| reset при gain change | ✅ |
| Live/AF isolation from still state | ✅ implementation |
| repeat old ~0.64 ↔ ~1.43 s sparse target | 🟡 |
| stable convergence | 🟡 HIL |
| LOCK without limit cycle | 🟡 |
| relock after real scene change | 🟡 |

## Scheduler

### Observation model

| Можливість | Стан |
|---|---|
| `ObservationPlan` | ✅ |
| `ObservationBlock` | ✅ |
| `dso-fits` | ✅ |
| `planetary-ser` | ✅ |
| `mosaic-fits` | ✅ |
| mixed block types | ✅ |

### Calendar / persistence

| Можливість | Стан |
|---|---|
| per-task date/time | ✅ |
| Start now / future start | ✅ |
| planning months/year | ✅ |
| calendar stored on node | ✅ |
| persistent completed-block cursor | ✅ |
| completed blocks not replayed after restart | ✅ |
| optional Park after | ✅ |
| auto-unpark before next task | ✅ |
| resume всередині unfinished block | ⬜ OAL 1.0 |

### GUI

| Можливість | Стан |
|---|---|
| Add/Edit/Delete/Clear | ✅ |
| Move up/down | ✅ |
| Use current telescope coordinates | ✅ |
| per-task date/time | ✅ |
| Park after | ✅ |
| Sky Map target → Scheduler | ✅ HIL |
| calendar/timeline view | ⬜ nice-to-have |
| overlap/conflict warning | ⬜ |
| drag-and-drop | ⬜ non-blocker |

### DSO automation

| Етап | Стан |
|---|---|
| Slew | ✅ |
| Solve | ✅ |
| Sync/Recenter | ✅ framework |
| Autofocus | ✅ |
| FITS × N | ✅ |
| autofocus every N | ✅ |
| solve/recenter every N | ✅ |
| scheduler camera preflight | ✅ |
| auto-stop interactive Live View | ✅ |
| full scheduler DSO HIL | 🟡 |
| ASTAP closed-loop real sky | 🟡 |
| guiding | ⬜ |
| dither | ⬜ |
| filter changes | ⬜ |
| meridian-flip recovery | ⬜ |

## Planetary automation

Поточний каркас: **GOTO → acquisition → planet detect → center → autofocus → ROI → SER**.

| Можливість | Стан |
|---|---|
| planetary block | ✅ |
| full-frame acquisition | ✅ |
| planet detection | ✅ |
| centering foundation | ✅ |
| autofocus stage | ✅ |
| hardware ROI | ✅ |
| multiple SER runs | ✅ |
| pauses | ✅ |
| fast ROI tracking | ✅ |
| `.roi.jsonl` provenance | ✅ |
| planet-lost detection | ✅ |
| optional slow mount correction | ✅ implementation |
| camera↔mount response calibration foundation | ✅ |
| real planetary scheduler HIL | 🟡 |
| automatic physical centering | 🟡 |
| ROI follows target | 🟡 |
| slow mount correction HIL | 🟡 |
| calibration-frame ROI reuse | ⬜ |
| long-session ephemeris refresh | ⬜ |

## Mosaic capture

| Можливість | Стан |
|---|---|
| `mosaic-fits` block | ✅ |
| rows × columns | ✅ |
| overlap % | ✅ |
| camera rotation | ✅ |
| FOV from sensor + optics | ✅ |
| tangent-plane tile calculation | ✅ |
| serpentine traversal | ✅ |
| slew per tile | ✅ |
| solve/recenter per tile | ✅ |
| optional AF per tile | ✅ |
| FITS × N per tile | ✅ |
| camera footprints | ✅ implementation |
| Sky Map mosaic overlay | ✅ implementation |
| Stellarium FOV export | ✅ implementation |
| real 2×2 mosaic HIL | 🟡 |
| overlay end-to-end HIL | 🟡 |
| mosaic manifest | 🟡 implementation |
| resume individual missing tiles | ⬜ OAL 1.0 |

## Polar Alignment

| Можливість | Стан |
|---|---|
| manual sample workflow | ✅ |
| automatic multi-solve workflow | ✅ |
| controlled RA slews | ✅ |
| polar-axis error estimate | ✅ |
| optional safe-sky region | ✅ |
| wrapped azimuth region | ✅ |
| full slew-path validation | ✅ |
| safe region OFF by default | ✅ |
| real-sky HIL | 🟡 |
| accuracy test | 🟡 |
| guided Alt/Az bolt instructions | 🟡 |
| iterative convergence | 🟡 |
| final verification solve | 🟡 |

## Astrometry

| Можливість | Стан |
|---|---|
| ASTAP integration | ✅ |
| solve operation | ✅ |
| scheduler solve | ✅ |
| Sync/recenter framework | ✅ |
| real-sky ASTAP qualification | 🟡 |
| ~1–2 arcmin acceptance | 🟡 |
| retries foundation | ✅ |
| failure recovery | 🟡 |
| alternative solver fallback | ⬜ |

## Guiding

| Можливість | Стан |
|---|---|
| guide-camera role | ✅ |
| simultaneous Guide Live View | ✅ implementation |
| calibration | ⬜ |
| guide-star detection | ⬜ |
| RA/DEC correction loop | ⬜ |
| direct pulse guide | ⬜ |
| dithering | ⬜ |
| settle | ⬜ |
| lost-star recovery | ⬜ |
| scheduler integration | ⬜ |

Guiding поки не hard blocker для першої supervised Beta, але лишається великою production-прогалиною.

## Інші observatory devices

API/profile foundation існує для Filter Wheel, Rotator, Dome/Roof, Weather, GPS, Power, Cover/Calibrator, Safety Monitor і Switch. Production real-device drivers/workflows — переважно **OAL 1.0**, а не blocker supervised Beta.

## Discovery / hotplug

| Можливість | Стан |
|---|---|
| async discovery | ✅ |
| explicit Refresh | ✅ |
| persisted bindings | ✅ |
| Gemini COM migration | ✅ HIL |
| predictable QHY hotplug | 🟡 |
| Canon hotplug | 🟡 |
| Gemini unplug/replug | 🟡 |
| stale GUI device cleanup | 🟡 |
| one-click Refresh guarantee | 🟡 |
| automatic reconnect policy | ⬜ |

## Website

| Частина | Стан |
|---|---|
| source website | ✅ |
| EN | ✅ |
| UA | ✅ |
| feature sections | ✅ |
| Project rationale / «Навіщо OAL» | ✅ source |
| Public manifesto | ✅ source |
| User/astronomer guide | ✅ source |
| OAL core developer guide | ✅ source |
| Third-party integration guide | ✅ source |
| Native driver SDK entry point | ✅ source |
| high-rate/WebRTC content | ✅ source |
| scheduler content | ✅ |
| Supported Hardware matrix | 🟡 |
| HIL Pending / Planned matrix | 🟡 |
| Protocol/API docs | ✅ foundation |
| Roadmap | ✅ |
| `openastro.link` DNS apex + `www` | ✅ live |
| HTTPS serving | ✅ live |
| Deploy current v0.2.10.58-buildfix9 site/docs | 🟡 live deployment stale |
| Downloads/Releases | ⬜ |

## Windows release

| Можливість | Стан |
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
| ZWO EAF import-lib repair | ✅ |
| Canon EDSDK x64 pair locking | ✅ |
| PE architecture validation | ✅ |
| warning cleanup | ✅ |
| fresh buildfix9 configure | ✅ 2026-09-08 |
| fresh MSVC buildfix9 | ✅ 2026-09-08 |
| `Linking CXX executable OpenAstroSuite.exe` | ✅ |
| optional Vulkan SDK | ⬜ not required |
| OpenCV/UVC node+Live View runtime smoke | ✅ previous `.58` lineage |
| Canon driver runtime load after buildfix9 | 🟡 immediate |
| ZWO EAF driver runtime load after buildfix9 | 🟡 immediate |
| portable release package | ⬜ |
| Qt/OpenCV release deployment | 🟡 foundation |
| WebRTC runtime deployment | ✅ build tree |
| vendor SDK runtimes | ✅ build tree/configure validation |
| clean-machine install test | ⬜ |
| installer/portable ZIP | ⬜ |
| release notes | ✅ development checkpoint |
| signed checksums | ⬜ release |

Static qualification retained for `.58`: **75/75 general scripts PASS**, **43/43 WebRTC PASS**, **43/43 high-rate/Dual-Live PASS**.

## Що зараз реально блокує 0.3 Beta

Старий blocker `Clean Windows build` закритий. Поточний P0:

1. 🟡 Canon + ZWO EAF node-registry runtime load regression після buildfix9.
2. 🟡 QHY high-rate HIL: MAX capture / Preview 60.
3. 🟡 SER 5–10 хв, `recordDropped=0`, AutoStakkert frame-count verification.
4. 🟡 Preview 15/30/60/MAX без зниження recording FPS.
5. 🟡 Actual WebRTC transport + failure/fallback HIL.
6. 🟡 Main + Guide Dual Live HIL.
7. 🟡 30–50 direct-Wi-Fi manual press/release/direction-change HIL.
8. 🟡 Scene autofocus HIL + repeatability.
9. 🟡 Sparse-scene still auto-exposure convergence/LOCK HIL.
10. 🟡 Scheduler DSO HIL.
11. 🟡 ASTAP solve + recenter HIL.
12. 🟡 Planetary SER/ROI executor HIL.
13. 🟡 Mosaic 2×2 + footprint overlay HIL.
14. 🟡 Polar Alignment real-sky HIL.
15. 🟡 Discovery/hotplug regression.
16. ⬜ Windows portable release + clean-machine test.
17. 🟡 Deploy **current** site/docs на `openastro.link` (DNS/HTTPS уже live; deployed content stale).

Mount geometry більше не Beta blocker. Windows compilation більше не blocker. WebRTC implementation більше не blocker; **physical transport/camera qualification — blocker**.

## Beta → OAL 1.0

Після supervised Beta лишаються: durable FITS/SER/block resume; TLS/auth/RBAC; persistent audit/event journal; production guiding+dither; automatic meridian flip; weather; roof/dome; power; emergency shutdown; unattended safety state machine; thermal focus compensation; filter wheel; calibration manager; disk/storage safeguards; resumable downloads; process-isolated drivers; public driver SDK/conformance suite; production RTP video codecs; Smart Telescope UX.
