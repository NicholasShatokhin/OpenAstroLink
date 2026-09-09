# OpenAstroLink v0.2.10.58 — checkpoint WebRTC preview transport

v0.2.10.58 додає optional native WebRTC transport для preview, зберігаючи high-rate acquisition/recording архітектуру v0.2.10.57.

## WebRTC

- `libdatachannel` реалізує PeerConnection і DataChannel.
- `/webrtc` — signaling WebSocket; `/events` лишається JSON control/state, `/video` — fallback OALV v1 WebSocket.
- `oalv-main` і `oalv-guide` — незалежні unordered, partially reliable DataChannel.
- OALW v1 фрагментує наявний OALV v1/JPEG preview packet. У цій ревізії **ще немає** заяви про RTP H.264/H.265/AV1 media track.
- Preview має latest-frame semantics і 2 MiB backpressure guard. Повільна мережа може drop-ати preview, але не повинна блокувати acquisition або SER recording.
- `OAL_WEBRTC_ICE_SERVERS` задає STUN/TURN URL через comma/semicolon/newline-separated список.
- Official Windows native presets bootstrap-лять і вимагають `libdatachannel`; Linux native bootstrap може per-user зібрати pinned v0.24.5.

## High-rate camera streaming

SER/raw recording і далі відбувається upstream від preview processing. JPEG кодується один раз у droppable preview worker, після чого той самий OALV packet може піти WebRTC або `/video` fallback.

## Qualification boundary

Source/static qualification відокремлена від hardware qualification. Потрібні fresh Windows build, QHY high-rate HIL, Dual Live HIL, network fallback HIL і 5–10 хв SER validation.

Mount direct-MC geometry v9 frozen та незмінена (`Axis1Sign=+1`, `Axis2Sign=-1`).

## Windows bootstrap buildfix2 (2026-09-07)

Під час першого запуску vcpkg bootstrap друкував службовий текст у PowerShell success pipeline. Оскільки `Ensure-Vcpkg` повертав root path через той самий pipeline, caller міг отримати status text разом із шляхом і спробувати виконати цей склеєний рядок. Тепер native stdout іде прямо в host, фізичний `vcpkg.exe` перевіряється через `Test-Path`, а запуск виконується через окремий `Join-Path`. Уже створений per-user clone vcpkg видаляти не потрібно.

## Windows native dependency bridge buildfix3 (2026-09-07)

Після успішного native dependency bootstrap прямий
`cmake --preset my-windows-observatory` все ще міг завершитися помилкою, що
`libdatachannel` не знайдено. Bootstrap коректно записував
`LibDataChannel_DIR` і vcpkg installation prefix у
`.oal/native-deps-windows-x64.json`, але цей record читав лише
`scripts/build_windows.ps1`. Прямий CMake preset залишав Qt-only
`CMAKE_PREFIX_PATH` із локального user preset.

Тепер native Windows configure автоматично імпортує dependency record, якщо він
існує і це не cross-build. Явні значення з preset/`-D` мають пріоритет; із
record заповнюються лише порожні або `*-NOTFOUND` cache entries. Vcpkg prefix
додається до `CMAKE_PREFIX_PATH`, щоб `libdatachannel` міг знайти свої
OpenSSL/libjuice/usrsctp package dependencies. Windows-шляхи з backslash у JSON
нормалізуються до CMake-style forward slashes.

Після успішного bootstrap тепер еквівалентні обидва шляхи:

```powershell
.\scripts\build_windows.ps1 -Preset my-windows-observatory -Clean
```

і з x64 MSVC developer environment:

```powershell
cmake --preset my-windows-observatory
cmake --build --preset my-windows-observatory --parallel
```

## Windows Qt 6.10 compile buildfix4 (2026-09-07)

Перший fresh Windows build із увімкненим WebRTC дійшов до native compilation і виявив Qt 6.10 source-compatibility error у diagnostic string підтвердження зупинки direct SynScan/EQDrive Wi-Fi. Виклик `QString::arg(axis, last)` змішував integer і `QString` у variadic overload, що Qt 6.10/MSVC відхиляє. Тепер використано еквівалентну chained форму `QString::arg(axis).arg(last)`.

Це лише compile fix для diagnostic string. Він **не змінює** mount geometry v9, polarity осей, Home/Park conventions, manual-slew commands, stop/retry semantics або serial/Wi-Fi parity.

## 2026-09-07 Windows compile progression — buildfix5

- ✅ WebRTC dependency discovery/configure вже HIL-build-підтверджено на Windows/MSVC: CMake знаходить `LibDataChannel::LibDataChannel` і генерує observatory build.
- ✅ buildfix4 прибрав Qt 6.10 mixed `QString::arg` compile blocker; наступний build дійшов до 63/85.
- 🛠️ buildfix5 виправляє два наступні blockers: WebRTC OALW fragmentation тепер нормалізує `QByteArray::size()` (`qsizetype`) перед `std::min`, а Windows linkage ZWO EAF більше не приймає `EAF_focuser-static.lib` для native driver.
- 🛠️ ZWO EAF bootstrap тепер віддає перевагу DLL import library; CMake може автоматично відновитися зі старого cached `*-static.lib`, знайти `EAF_focuser.lib`/`EAFFocuser.lib` і, за наявності, спарити runtime DLL directory з тієї ж SDK директорії.
- 🔒 Mount geometry v9, axis polarity, Home/Park та SynScan/EQDrive motion semantics не змінені.
- 🟡 Після buildfix5 ще потрібен fresh Windows compile, перш ніж v0.2.10.58 можна назвати build-qualified.

## Windows vendor runtime ABI buildfix8 (2026-09-07)

Після успішного Windows build з WebRTC і runtime smoke node native Canon EDSDK та ZWO EAF plugins дали `ERROR_BAD_EXE_FORMAT`. buildfix8 посилює Windows vendor staging: архітектура визначається з PE COFF Machine самої DLL, а не з назви SDK-папки; дозволяється лише target architecture, stale однойменні DLL видаляються з executable root і `drivers/`, а configure завершується помилкою, якщо runtime directory містить лише несумісні DLL. Bootstrap також вибирає AMD64 QHY/ZWO/Canon runtime за PE metadata. `scripts/repair_windows_vendor_runtime.cmd` може виправити вже зібране Windows tree in-place. Mount geometry v9 не змінена.

### Windows buildfix9 (2026-09-08)
Windows staging Canon EDSDK тепер жорстко спарює вибрану import library з sibling runtime directory. Тобто x64 `EDSDK_64/Library/EDSDK.lib` більше не може бути спарена з паралельним x86 деревом `EDSDK/Dll`. PE machine runtime перевіряється, а stale/mismatched cache автоматично виправляється.

## Windows buildfix9 qualification (2026-09-08)

Точний current Windows x64/MSVC tree тепер успішно configure/build з увімкненим WebRTC. Configure log підтверджує `LibDataChannel::LibDataChannel`, stage п'яти WebRTC runtime DLL, same-architecture QHY/ZWO runtimes, repair ZWO EAF на DLL import library і pairing `EDSDK_64/Library/EDSDK.lib` із sibling AMD64 `EDSDK_64/Dll`. Build доходить до `Linking CXX executable OpenAstroSuite.exe` без compile/link errors.

Це закриває clean-Windows-build gate для `.58`. Runtime qualification corrected Canon і ZWO EAF plugin load ще pending: наступний node startup має показати `oal.canon` і `oal.zwo.eaf` у native registry без Win32 error 193. WebRTC/QHY high-rate/Dual-Live HIL теж pending. Повний актуальний список gates — `CURRENT_CHECKLIST.md`.


## Public website/documentation update (2026-09-09)

Repository site source тепер публікує чітке пояснення, що таке OpenAstroLink, навіщо його створено та чому проєкту була потрібна інша архітектурна комбінація, ніж дають наявні astronomy stacks. Також опубліковані public manifesto та окремі documentation entry points для astronomers/users, OAL core contributors, third-party applications і native driver authors. DNS/HTTPS для `openastro.link` та `www.openastro.link` уже live; актуальна website-задача — deploy current source поверх старішого live snapshot.
