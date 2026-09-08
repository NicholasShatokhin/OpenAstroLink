# High-rate camera streaming та OALV v1

> Поточний synchronized snapshot: **v0.2.10.58 (2026-09-07)**. Див. `STATUS.md` та `RELEASE_0.2.10.58.md` щодо current qualification boundaries.


## Мета

OpenAstroLink має записувати кожен кадр, який реально видає планетарна камера, і водночас показувати responsive Live View. Preview можна пропускати; raw acquisition та SER recording — ні. Main і guide cameras також повинні працювати одночасно для юстировки оптичних осей і підготовки guiding.

## Pipeline

```text
native camera SDK
      |
      v
acquired raw frame
      |
      +--> SER writer (кожен acquired frame, raw)
      |
      +--> analysis sampler (rate-limited за потреби)
      |
      +--> latest-only preview worker
                |
                +--> optional debayer / display stretch / resize
                +--> local Qt image
                +--> remote JPEG -> OALV /video
```

`captureFpsLimit` та `previewFpsLimit` навмисно незалежні. Нуль означає, що OAL не накладає власного FPS limit. Фізичними межами залишаються camera, USB controller, CPU, memory bandwidth і storage.

## OALV v1 packet

```text
0..3   "OALV"
4      version = 1
5      role: 0 main, 1 guide
6..9   uint32 big-endian довжина JSON header
10..   JSON header
...    JPEG payload
```

JSON header містить щонайменше role/frame identity, capture timestamp, dimensions, exposure, gain, measured capture/record/preview rates, preview/record drop counters, requested limits, preview encoding parameters і transport name. У v0.2.10.57 `width`/`height` явно означають dimensions encoded JPEG payload після preview-only scaling, а `sourceWidth`/`sourceHeight` зберігають dimensions до transport downscale.

## WebSocket routing

OAL використовує один налаштований WebSocket port, але різні URL paths:

- `ws://host:port/events` — text JSON events;
- `ws://host:port/video` — binary OALV preview packets.

Невідомий/legacy path трактується як event client для compatibility.

## Backpressure policy

Preview latest-only. Для кожної camera role одночасно має бути не більше одного preview conversion/encode. Якщо worker ще зайнятий, пропускається тільки preview opportunity; raw/SER acquisition продовжується. WebSocket sender також не накопичує необмежену чергу для повільного клієнта.

У поточній Beta SER writing synchronous, тому storage, який не витримує raw data rate, все ще може обмежити acquisition. Qualification показує capture/record FPS і вимагає `recordDropped = 0`; bounded asynchronous recording ring можна додати в OAL 1.0, якщо HIL покаже storage stalls.

## Main + guide

Main і guide Live View мають окремі locks. Dual Live підтримується, коли це різні фізичні cameras. UI не повинен створювати враження, що одну фізичну камеру можна одночасно захоплювати двома operations.

## Science integrity

Preview transformations лише display-only. Debayer, JPEG, scaling та 8-bit display conversion не змінюють FITS/RAW science files або raw SER payloads.



## WebRTC preview transport (v0.2.10.58)

Коли доступний `libdatachannel`, той самий post-recording OALV/JPEG preview packet може йти WebRTC data plane. `/webrtc` — Qt WebSocket signaling endpoint. Node є offerer і створює `oalv-main` та `oalv-guide` DataChannel; remote GUI формує answer і reassemble-ить OALW v1 fragments. Channels unordered і partially reliable (`maxPacketLifeTime=150 ms`).

OALW v1 має fixed 24-byte header і 48 KiB payload fragments. Receiver тримає latest-sequence state окремо для Main і Guide, тому incomplete старий preview frame може бути відкинутий одразу після появи новішого sequence. 2 MiB buffered-amount threshold drop-ає preview замість network backpressure на acquisition.

`/video` лишається binary OALV v1 WebSocket fallback. Він закривається лише коли готові обидва role DataChannel; якщо одна роль втрачає WebRTC, `/video` повертається і тільки ця роль споживає fallback frames. `OAL_WEBRTC_ICE_SERVERS` задає optional STUN/TURN URL.

Це WebRTC **transport v1**, а не RTP codec pipeline: payload поки OALV v1/JPEG. H.264/H.265/AV1 media tracks — наступна optimization після transport HIL.

## v0.2.10.57 pre-HIL hardening

Перед physical high-rate qualification audit snapshot v0.2.10.56 знайшов і виправив чотири ризики data plane:

- QHY більше не публікує вигаданий fixed `maxFps:30`; реальний camera maximum визначають exposure, ROI/binning, USB path і hardware.
- OALV payload dimensions більше не неоднозначні, коли `previewMaxWidth` зменшує preview.
- Remote controller виконує один JPEG decode на OALV frame замість незалежного повторного decode через Qt та OpenCV.
- Один native physical camera не можна відкрити одночасно як Main і Guide; Dual Live вимагає різних native device identities.

QHY також повторно використовує live buffer, sized у `camera.liveStart`, і звертається до SDK за memory length лише як fallback, якщо start-time size був недоступний.

## v0.2.10.57 qualification boundary

Implementation уже є, але current-revision HIL ще pending. Перший acceptance run: camera-max acquisition + 60 FPS preview, потім 5–10 minute raw SER із zero record drops, після цього simultaneous main+guide Dual Live. Preview backpressure може drop-ати preview frames; воно не повинно тихо throttle-ити або drop-ати recording frames.

## Поточна qualification — 2026-09-08

High-rate/WebRTC source path тепер fresh Windows build-qualified. Залишився physical HIL: QHY `Capture FPS=MAX`/Preview-60 throughput, SER 5–10 хв з zero record drops, Preview 15/30/60/MAX independence і simultaneous Main+Guide streams. Див. `CURRENT_CHECKLIST.md` та `VALIDATION.md`.

