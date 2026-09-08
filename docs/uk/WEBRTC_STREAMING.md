# WebRTC preview transport — v0.2.10.58

> Поточна qualification: source/static qualified; fresh native build та physical/network HIL pending.

## Призначення

WebRTC — low-latency remote preview transport downstream від camera acquisition і SER recording. Він не володіє камерою і не має права створювати backpressure на scientific acquisition.

## Topology

- `/events`: JSON state/events.
- `/webrtc`: WebSocket signaling для SDP/ICE.
- `oalv-main`: WebRTC DataChannel для Main preview.
- `oalv-guide`: WebRTC DataChannel для Guide preview.
- `/video`: binary OALV v1 WebSocket fallback.

Node — WebRTC offerer, GUI — answerer. Optional STUN/TURN URL задаються через `OAL_WEBRTC_ICE_SERVERS` як список, розділений comma, semicolon або newline.

## Payload

v0.2.10.58 навмисно повторно використовує OALV v1/JPEG. Уже закодований OALV packet ділиться на OALW v1 fragments. OALW має 24-byte header: magic/version, role, flags, frame sequence, total OALV size, fragment index і fragment count. Payload fragment — максимум 48 KiB.

Це справжній WebRTC PeerConnection/DataChannel transport, але ще не RTP H.264/H.265/AV1 media-track pipeline.

## Drop/backpressure policy

DataChannel unordered і partially reliable з `maxPacketLifeTime=150 ms`. Якщо в DataChannel buffered більше 2 MiB, current preview frame пропускається. Receiver застосовує latest-sequence semantics окремо для Main і Guide, тому incomplete старі frames можуть бути відкинуті після приходу новішого sequence.

Ці drops стосуються тільки preview. Raw SER append лишається upstream від preview processing.

## Fallback

GUI спочатку відкриває `/video`, поки WebRTC negotiate-иться. `/video` закривається лише коли відкриті Main і Guide DataChannel. Якщо один channel стає unavailable, `/video` відновлюється. WebSocket frame для ролі з healthy WebRTC DataChannel ігнорується, тому transition не дає duplicate delivery.

## Dependency/bootstrap

- Windows: `bootstrap_native_dependencies.ps1` ставить `libdatachannel:x64-windows` через per-user vcpkg, official native preset вимагає WebRTC.
- Native Linux: `bootstrap_native_dependencies.sh` знаходить installed CMake package або per-user збирає pinned libdatachannel v0.24.5 з вимкненими media/WebSocket helpers.
- Generic/custom builds: `OAS_ENABLE_WEBRTC=ON` вмикає discovery; `OAS_REQUIRE_WEBRTC=ON` робить відсутність library fatal. Без library і без require flag `/video` лишається доступним.

## HIL acceptance

1. `node/info` показує `webrtc.available=true`.
2. Main і Guide переходять у `networkTransport=webrtc-datachannel`.
3. Втрата одного channel повертає лише цю роль на `/video` без freeze іншої ролі.
4. Network congestion збільшує preview drops, але не зменшує capture/record rate; `recordDropped=0`.
5. Dual Live зберігає незалежні exposure/gain/rates.
6. 5–10 хв SER лишається frame-count consistent в AutoStakkert.

## Поточна qualification — 2026-09-08

Windows x64/MSVC buildfix9 fresh-build qualified з `LibDataChannel::LibDataChannel`; п'ять vcpkg WebRTC runtime DLL stage-яться у build tree. Node має попередній `.58` runtime-start/OpenCV-UVC Live View smoke pass. Actual `webrtc-datachannel` negotiation, fallback/reconnect, QHY high-rate throughput і Main+Guide Dual Live ще HIL gates. Див. `CURRENT_CHECKLIST.md`.

