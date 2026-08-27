# Нативна підтримка EQDrive

Цей документ описує експериментальний нативний драйвер OpenAstroLink для контролерів EQDrive.

## Обсяг

`oal.eqdrive` — новий mount-драйвер ABI v2, окремий від наявного `oal.skywatcher` / EqMount. Наявний Sky-Watcher драйвер збережено без заміни для SynScan-сумісного обладнання.

Нативний шлях напряму працює з EQDrive через serial/VCP за опублікованим протоколом ASTEP і не потребує EQMOD чи ASCOM.

Джерело протоколу: <https://www.eqdrive.com.ua/en/support/eqdrive-protocol>.

## Пошук пристрою

Автоматичний discovery навмисно консервативний: насамперед перевіряються CP210x/EQDrive-подібні serial-пристрої. Порт можна зафіксувати явно:

```text
openastrolink-node.exe --eqdrive-port COM5
```

або вибрати в **Devices → Native serial discovery → EQDrive (ASTEP)**.

Focused discovery перебирає обмежений набір baud rate та станів DTR/RTS і використовує лише read-only ASTEP-команди (`St`, запити firmware, `Cg`). Discovery ніколи не рухає монтування.

Backend має вигляд:

```text
native:oal.eqdrive/eqdrive:<stable-id>
```

За можливості як стабільний ID використовується серійний номер USB-UART, а не номер COM-порту.

## Реалізовані ASTEP-операції

Використовуються публічні команди mount-секції ASTEP:

- `St` — стан осей, позиції та швидкості;
- `Pos` — позиції осей;
- `Slew` — відносний рух;
- `Goto` — для best-effort зупинки шляхом перенаведення в поточну позицію;
- `Speed` — трекінг і тимчасові guide-rate зміщення;
- `Vin`, `Vbs`, `Cg` — телеметрія/читання конфігурації;
- `FW`, `FWs`, `FWx` — необов'язкові запити ідентичності.

У ASTEP координати/зміщення осей передаються в градусах, швидкості — у градусах за годину.

## Модель небесних координат у v0.2.10.16

Драйвер навмисно не вгадує механічний home, pier side чи правило meridian flip для ще не кваліфікованої установки. Замість цього використовується консервативна модель **sync-anchor-v1**.

1. Під'єднати монтування.
2. Навести його на відому позицію на небі перевіреним способом.
3. Виконати один **Sync** OpenAstroLink з відомими RA/Dec.
4. Після появи anchor OAL перетворює зміни ASTEP-координат осей у небесні координати та приймає RA/Dec GOTO в межах тієї ж гілки pier-side.

До першого Sync `mount.status` повертає `coordinateValid=false`. GUI показує **Sky coordinates: not synced yet**, adaptive plate solving не використовує хибну позицію mount як hint, а Stellarium bridge не публікує фіктивні `0,0`.

### Поточні обмеження

У цій версії ще не заявлені як HIL-кваліфіковані:

- автоматичне визначення pier side;
- автоматичний meridian flip;
- нативна геометрія park/unpark;
- збереження небесної alignment-моделі після довільного механічного переміщення;
- підтвердження знаків/орієнтації на всіх конфігураціях EQDrive.

Це потрібно перевірити на реальному монтуванні після підтвердження базового serial-control.

## Телеметрія

ABI додатково експонує `eqdrive.telemetry`: позиції/швидкості осей, стан драйверів, firmware, serial port/baud rate та best-effort читання `Vin`, `Vbs`, `Cg`.

## Взаємодія з Classic ASCOM

Classic ASCOM/EQMOD лишається повноцінним compatibility path у Windows — і для реального використання, і як еталон поведінки під час HIL нативного EQDrive. Див. [ASCOM_CLASSIC.md](ASCOM_CLASSIC.md).
