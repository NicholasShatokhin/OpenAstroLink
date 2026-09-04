# Як було зведено попередні фрагменти


> **Контекст поточного релізу:** v0.2.10.50. Це історичні notes про консолідацію; native OAL лишається default, INDI — opt-in.

Цей пакет не є буквальним накладанням усіх diff-патчів, створених під час розробки. Кілька класів перевизначалися суперечливими способами, деякі бекенди, позначені як «готові», фактично були заглушками, а консольний і Qt-код містили дубльовану логіку. Під час консолідації свідомо зроблено такі зміни.

## Єдине ядро пристроїв

- `CaptureDevice`, кілька версій `MountController` і `FocuserController` замінено спільними інтерфейсами `ICamera`, `IMount`, `IFocuser`.
- GUI, алгоритми, OAL HTTP server та OAL WebSocket використовують ті самі активні об'єкти через `ApplicationController`.
- Немає окремої «серверної» симуляції, яка може непомітно розійтися зі станом GUI.

## Кросплатформна реалізація

- POSIX `termios/open/read/write` замінено на Qt `QSerialPort`, тому serial LX200 compatibility backend не прив'язаний до Linux.
- Класичний Windows COM ASCOM не є обов'язковим; interoperability з ASCOM забезпечується насамперед через Alpaca HTTP.
- QHY, Canon EOS, ZWO ASI та ZWO EAF винесені в native OAL ABI-v2 plug-ins. `oal.canon` використовує libgphoto2 лише як linked USB/PTP transport. Legacy `canon-gphoto2`, INDI, Alpaca та LX200 залишаються optional compatibility integrations, а не reference architecture.

## Виправлення протоколів

- Alpaca setter/action endpoints використовують HTTP `PUT` та form-encoded parameters замість GET-команд із query string.
- `sync` і `slew` розділені; slew не виконує sync неявно.
- Поточна REST-реалізація OAL використовує спільний envelope `{ok,error,data}` та typed WebSocket events до запланованого переходу на RFC 9457.

## Виправлення математики

- Старий «plate solver», який завжди повертав Полярну, вилучено. Його замінили реальний, але обмежений triangle/catalog prototype, production adapter ASTAP та окремий neural-solver interface.
- Старе наближення polar alignment через усереднення `solved - mount` вилучено. Поточний estimator визначає механічну вісь RA з відносних 3D-орієнтацій solved кадрів.
- Motion estimator зіставляє зоряні центроїди через robust partial affine transform.
- Autofocus використовує coarse/fine scans, кілька кадрів на позицію та окремі метрики для зірок і планет.

## Що навмисно не подається як завершене

- Власний full-sky blind solver, geometric Bahtinov spike-offset solver, durable scheduler executor та neural runtime ще не завершені.
- Native QHY, Canon EOS, Gemini, Sky-Watcher, ZWO ASI та ZWO EAF потребують HIL-перевірки на конкретних пристроях/firmware перед production label.
- ABI-v2 manifest discovery та generic native camera/mount/focuser adapters інтегровані, але sandboxed out-of-process hosting і фінальна normative capability/conformance model ще попереду.
- Dual-camera ownership реалізовано, але production guide-camera calibration/dither/star-loss pipeline ще не завершений.
- Stellarium bridge навмисно реалізує стандартний telescope-control scope — mount position і GOTO — а не весь OAL observatory API.
