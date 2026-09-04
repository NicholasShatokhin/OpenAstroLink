# Native EQDrive support — v0.2.10.50

`oal.eqdrive` — native ABI-v2 driver для EQDrive. Він підтримує два low-level transport paths, але астрономічна геометрія лишається в OAL Core:

- direct Sky-Watcher/EQMOD Motor Controller через serial;
- public EQDrive ASTEP mount protocol як fallback/alternate transport.

Direct-MC coordinate model **v9 HIL-підтверджений** на реальному монтуванні. Геометрія, signs та Home/Park convention frozen, доки нові HIL-дані не доведуть протилежне.

## Canonical serial protocol: ASTEP

ASTEP path використовує public EQDrive astronomical-equipment protocol. Для movement-free discovery/telemetry OAL використовує mount-specific `St`, `Pos`, `Cg`, а для руху — `Speed`, `Slew`, `Goto`, `Drv`. Axis position/displacement задаються у mechanical degrees, швидкість — degrees/hour.

Canonical reference: <https://www.eqdrive.com.ua/en/support/eqdrive-protocol>.

Discovery навмисно не рухає mount. Device оголошується mount лише після валідної mount-specific відповіді. CP210x/SILABS ports мають пріоритет; port можна зафіксувати, наприклад:

```text
openastrolink-node.exe --eqdrive-port COM5
```

Якщо OS повідомляє busy/access-denied, OAL не бореться з EQMOD або іншим власником за той самий COM port.

## Direct Motor Controller coordinate model v9

Для direct Motor Controller counts, прочитані у підготовленій physical polar Home pose, стають session mechanical reference `Axis1=0°, Axis2=0°`. Raw controller counts — лише diagnostics; sky↔mechanical GEM geometry належить OAL Core.

HIL-qualified mapping:

```text
Axis1Sign = +1
Axis2Sign = -1
coordinate model = eqmod-gem-ha-dec-v9
```

Native EQDrive serial і direct SynScan/EQDrive Wi-Fi використовують одну Core v9 geometry та shared Motor Controller GOTO planning. Відрізняється I/O transport, а не astronomical kinematics.

Frozen GEM equations та Home/Park convention описані в [MOUNT_GEOMETRY.md](MOUNT_GEOMETRY.md).

## GOTO safety після HIL qualification

У v0.2.10.50 видалено **тимчасовий прихований driver-level `maxNativeGotoDeg` qualification envelope**. Він існував лише на час перевірки axis/pier direction і не повинен лишатися після успішного v9 HIL.

Замість дубльованого прихованого ліміту є два явні safety layers:

1. **Core/profile sky safety** — `maxGotoSkyDeltaDeg` (legacy alias `maxGotoAxisDeltaDeg`) задає operator-controlled реальну кутову відстань по небу. Для обережного supervised HIL її можна лишити малою, а для звичайного full-range GOTO підняти до 180°.
2. **Raw-axis request guard** — `mount.gotoAxes` зберігає явний per-request `maxAxisDeltaDeg` mechanical ceiling, default 180° shortest-axis envelope. Це захищає raw mechanical команди без прихованого обмеження normal RA/DEC GOTO у transport driver.

При видаленні temporary cap **жодна** v9 geometry/sign/Home/Park logic не змінювалася.

## Park і meridian behavior

Mechanical Home/Park persistence належить OAL Core/profile, а не окремій прихованій celestial model EQDrive. Automatic meridian-flip orchestration ще не входить до unattended-qualified scope; supervised GOTO більше не має штучного qualification gate у драйвері.

## Direct Wi-Fi — transport peer, не ASTEP

`synscan-wifi` напряму працює із SynScan-compatible Wi-Fi/EQDrive Motor Controller по UDP 11880. Це не ASTEP і не SynScan App/Pro API (`synscan-app`, UDP 11881). Direct serial та direct Wi-Fi все одно мають спільні coordinate model v9 і low-level GOTO-plan semantics.

Див. [SYNSCAN_NETWORK.md](SYNSCAN_NETWORK.md).
