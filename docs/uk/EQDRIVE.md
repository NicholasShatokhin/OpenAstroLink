# Native підтримка EQDrive

`oal.eqdrive` — експериментальний native ABI-v2 драйвер для контролерів EQDrive.
Він залишається окремим від наявного `oal.skywatcher` / EqMount.

## Канонічний serial protocol: ASTEP

Native serial driver використовує публічну специфікацію **EQDrive ASTEP
(astronomical equipment protocol)**. Mount-секція прямо визначає read-only
`St`, `Pos`, `Cg`, telemetry напруги та команди руху `Speed`, `Slew`, `Goto`,
`Drv`. Позиції/зміщення осей передаються у градусах, швидкості — у градусах/год.

Канонічне джерело: <https://www.eqdrive.com.ua/en/support/eqdrive-protocol>.

Discovery не рухає монтування і пробує mount-specific команди в такому порядку:

- `St\r` — стан, позиції/швидкості обох осей і біти GOTO/driver;
- `Pos\r` — позиції двох осей;
- `Cg\r` — конфігурація двигунів, включно з Direction/Reversed;
- `FWx/FWs/FW` — лише додаткова identity-діагностика.

Пристрій з'являється як mount тільки після валідної ASTEP-відповіді `St` або
`Pos`.

## Discovery

CP210x/SILABS-порти мають пріоритет автоматично. Явний порт:

```text
openastrolink-node.exe --eqdrive-port COM5
```

Focused discovery перебирає baud list і всі чотири DTR/RTS combinations. Якщо
Windows повертає `Access is denied`, probe припиняється: EQMOD/Classic ASCOM,
EQDrive Config і native driver не можуть одночасно володіти одним COM-портом.
Після першого успішного discovery descriptor лишається у native-каталозі, поки
фізичний COM існує, навіть якщо його тимчасово відкрив EQMOD.

## Безпечна модель координат і руху

ASTEP віддає механічні градуси осей, а не небесні RA/DEC. OpenAstroLink
використовує `sync-anchor-v2` і не вигадує home/pier model:

1. підключити native EQDrive;
2. навести mount на відому небесну точку;
3. один раз виконати Sync;
4. після цього RA/DEC status/GOTO обчислюються від axis/sky anchor.

До Sync `coordinateValid=false`. Перший HIL-реліз обмежує один native GOTO
параметром `maxNativeGotoDeg` (default 15 deg). Ціль перетворюється в абсолютну
ASTEP-команду `Goto <axis1> <axis2>`. Налаштування Direction/Reversed уже
застосовує firmware EQDrive; OAL читає та показує їх, але не інвертує вдруге
навмання.

Park/meridian flip поки вимкнені до HIL-кваліфікації pier model. Classic
ASCOM/EQMOD залишається перевіреним fallback для великих slew.

## Direct Wi-Fi — окремий transport

`synscan-wifi` **не** використовує ASTEP. Він повторює прямий шлях SynScan Pro
до SynScan-compatible Wi-Fi adapter: Sky-Watcher Motor Controller protocol через
UDP 11880. Це окремо від serial `oal.eqdrive` ASTEP і окремо від `synscan-app`
(API запущеного SynScan Pro). Див. [SYNSCAN_NETWORK.md](SYNSCAN_NETWORK.md).
