# Gemini EAF — v0.2.10.11

Канонічний документ: `../GEMINI_EAF.md`.

Основний шлях — native `oal.gemini` через перевірений serial/MyFocuserPro2-compatible transport. INDI/Alpaca лишаються fallback. Capabilities визначаються реальним handshake/firmware, неперевірені команди не оголошуються підтриманими. Особлива увага — достовірності absolute position після power cycle.

## Поточний HIL-статус

Native `oal.gemini` тепер є основним шляхом для сумісного Gemini EAF. На реальному Windows/USB-serial обладнанні підтверджені discovery і connection, handshake `:02# → EOK#`, status/temperature traffic, фактичний рух фокусера та рух під час node-owned autofocus. INDI/Alpaca залишаються fallback.

Ще треба кваліфікувати position після reconnect/power-cycle, механічні limits, HALT/disconnect races, довготривалу стабільність і збіжність/повторюваність autofocus на реальній оптичній цілі.


## Пошук і ручний вибір послідовного порту (v0.2.10.11)

Нативний драйвер `oal.gemini` використовує три рівні вибору порту у такому порядку:

1. Явне перевизначення процесу (`OAL_GEMINI_PORT` або `openastrolink-node --gemini-port COM4`).
2. Порт, збережений у панелі OpenAstroSuite **Devices → Native serial discovery**.
3. Автоматичний перебір усіх послідовних портів, які повертає Qt (`QSerialPortInfo::availablePorts()`).

У GUI виберіть **Gemini EAF**, потім **Auto — scan all serial ports** або конкретний `COMx`/`/dev/tty*` і натисніть **Apply & rediscover native devices**. Якщо GUI керує віддаленою нодою, список портів надходить саме з ноди, а не з комп'ютера GUI.

Нода також надає `GET /api/v1/system/serial-ports` та `GET/POST /api/v1/drivers/serial-port`, тому вибір порту працює і для віддалених клієнтів. Вибір зберігається через `QSettings`; явний параметр командного рядка або змінна середовища ноди має пріоритет під час запуску.

Важливо: правильний вибір порту не виправляє помилку на рівні протоколу. Поточний нативний Gemini-handshake надсилає MyFocuserPro2 `:02#` на 9600 8N1 і очікує `EOK#`. Якщо контролер узагалі не відповідає, discovery коректно не класифікує цей порт як Gemini, і треба окремо діагностувати serial/protocol path.

### Windows/CH340: reset-aware handshake (v0.2.10.11)

Якщо контролер перезапускається під час відкриття COM-порту, `oal.gemini` спочатку тримає порт без команд протягом `openSettleMs` (зараз 2200 мс), після чого надсилає перший `:02#`. Якщо відповіді немає, той самий порт залишається відкритим ще на `resetRecoveryMs` (зараз 1200 мс) перед повтором. Це відповідає реальній HIL-поведінці Gemini EAF, де ручний тест без паузи не отримував відповіді, а після 2 с повертав `EOK#`.


### Serial-діагностика в `oal-hardware-probe` (v0.2.10.11)

Якщо Gemini зафіксовано через `--gemini-port`, probe тепер синхронно друкує лог драйвера навіть без запуску Qt event loop. У виводі видно помилку відкриття порту, першу та recovery-спроби `:02#`, отримані байти у текстовому та hex-вигляді, а також деталі transport timeout.

У Windows COM-порт зазвичай ексклюзивний. Якщо PowerShell/.NET-об'єкт `SerialPort` залишився відкритим через те, що `finally` не виконався, OAL не зможе відкрити той самий порт. Перед повторним тестом виконайте `$port.Close()`, завершіть процес serial-terminal, який тримає порт, або перепідключіть USB serial-пристрій.

## v0.2.10.11 — виправлення таймінгів у manifest

Windows HIL локалізував помилку пріоритету конфігурації у v0.2.10.8/v0.2.10.9. Хоча C++ default драйвера Gemini було збільшено для відновлення CH340 після відкриття порту, `oal_driver_gemini.manifest.json` досі задавав `openSettleMs: 150`, а host передає об'єкт `config` з manifest у драйвер під час старту. Тому runtime фактично продовжував надсилати probe приблизно через 150 мс після відкриття порту.

v0.2.10.11 синхронізує manifest і C++ defaults: `openSettleMs: 2200`, `resetRecoveryMs: 1200`. Перший `:02#` надсилається тільки після quiet-open інтервалу; при невдачі той самий порт залишається відкритим ще 1200 мс перед одним повтором. `oal-hardware-probe` також друкує ефективну serial-конфігурацію, щоб HIL-лог однозначно показував активні значення.
