# Guide користувача OpenAstroLink та астронома

> Поточний статус: supervised development/Beta qualification. Цей guide пояснює operating model і вже доступні можливості; точний HIL status завжди звіряйте з `CURRENT_CHECKLIST.md`.

## Що запускається

OpenAstroLink має дві ключові частини:

- **`openastrolink-node`** — володіє cameras, mounts і focusers та виконує operations/workflows;
- **OpenAstroSuite** — reference local/remote graphical client.

GUI може працювати на тому самому комп'ютері або remote. Stellarium може незалежно підключатися до telescope bridge.

## Для кого це

- planetary/deep-sky/visual astronomers, яким потрібен один control layer для mixed hardware;
- users, які хочуть observatory node на Windows/Linux/Raspberry Pi;
- users, яким потрібен один state equipment для local і remote control;
- experimenters, яким потрібен native hardware access із збереженням compatibility options.

## Поточний hardware напрям

Native drivers є для QHY, Canon EOS, ZWO ASI, ZWO EAF, Gemini, Sky-Watcher/EQDrive та simulated reference driver. Частина hardware HIL-qualified, частина лише implementation/build-qualified. Website і master checklist розділяють ці стани.

## Перший setup

1. Install/build node і GUI.
2. Запустити `openastrolink-node`, потім OpenAstroSuite.
3. Refresh/discover hardware та призначити camera/mount/focuser.
4. Налаштувати observing site та optical train.
5. Для mount перевірити backend і робити перші рухи тільки supervised.
6. Перед Scheduler зробити короткий interactive camera exposure.

## Live View/high-rate capture

Camera pipeline відділяє capture/recording від preview. `Capture FPS = MAX` означає відсутність штучного OAL cap. Preview має незалежний limit 15/30/60/MAX.

Planetary SER записується upstream від preview processing. Preview frame може drop-нутися під load, але healthy recording повинен мати `recordDropped=0`.

## Main і guide cameras

Main/guide roles мають незалежні locks/settings. Dual Live може одночасно показувати дві різні physical cameras — корисно для optical alignment та підготовки guiding.

## Mount control

Є GOTO, tracking, park/home, sync, abort, manual motion, Stellarium та built-in Sky Map. Direct-MC v9 geometry HIL-qualified для підтвердженого EQDrive/SynScan profile і не потребує ручного «перетлумачення» користувачем.

## Autofocus

Є Scene contrast AF та astronomical/star AF foundation. Scene AF містить exposure metering, tiled contrast, compact coarse/fine search, repeatability checks і rollback на failure/cancel. Real-scene/night repeatability ще Beta HIL gate.

## Auto Exposure

Still controller використовує bright-tail statistics для sparse scenes і LOCK/hysteresis. Він проходить фінальну HIL qualification — результат треба перевіряти, а не вважати production-qualified автоматично.

## Scheduler/workflows

Scheduler підтримує DSO FITS/RAW, planetary SER і mosaic blocks. DSO може slew, solve/recenter, autofocus і capture. Planetary має acquisition/ROI/SER foundation. Mosaic та Polar Alignment реалізовані, але мають pending HIL gates.

## Дані

Забезпечте достатньо storage. SER/FITS/RAW і sidecar/provenance metadata мають залишатися usable поза OpenAstroSuite. Для довгих planetary sessions контролюйте measured FPS і drop counters.

## Remote use

Pre-1.0 node призначений для trusted LAN/VPN. Не forward-те development HTTP/WebSocket ports прямо в Internet. TLS/auth/RBAC/audit заплановані до OAL 1.0.

## Safety

Найближча Beta — **supervised**. При тестах mount motion зберігайте physical access або independent stop path. Current Scheduler/Polar Alignment не є unattended roof/weather safety system.

## Troubleshooting

1. Переконатися, що node стартує без missing/wrong-architecture DLL errors.
2. Перевірити native-driver registry/discovery log.
3. Refresh devices.
4. Перевірити device interactively до Scheduler.
5. При slow Live View окремо порівняти capture FPS, preview FPS і drop counters.
6. При remote preview визначити, чи активний WebRTC, чи `/video` fallback.
7. Зберігати logs; вказувати exact device/firmware/backend і failed operation.

## Status references

- `CURRENT_CHECKLIST.md` — current feature/HIL matrix.
- `VALIDATION.md` — acceptance gates.
- `BUILD_PLATFORMS.md` — platform/build details.
- `HIGH_RATE_STREAMING.md` / `WEBRTC_STREAMING.md` — camera transports.
- `MOUNT_GEOMETRY.md` — direct-MC geometry evidence.
- `SCHEDULER.md` — scheduler model.
