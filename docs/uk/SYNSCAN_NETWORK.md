# Мережеві транспорти SynScan

OpenAstroLink навмисно розділяє два різні network backend, тому що Wi-Fi
адаптер монтування та застосунок SynScan App/Pro є різними protocol endpoint.

## Прямий Wi-Fi монтування — `synscan-wifi`

### Transport polarity для EQDrive Wi-Fi

OpenAstroLink відокремлює **transport polarity** Motor Controller від небесної геометрії монтування. Для generic Sky-Watcher UDP/11880 використовується protocol default `Axis1=+1`, `Axis2=+1`. HIL-кваліфікований EQDrive Wi-Fi профіль, що повідомляє `9216000` counts/rev на обох осях і timer frequency `53694`, використовує `Axis1=+1`, `Axis2=-1`; це виправляє фізичний напрямок DEC, не змінюючи v6 sky transform та startup Home/Park reference.

Для нестандартних адаптерів signs можна явно задати перед запуском node:

```text
OAL_SYNSCAN_WIFI_AXIS1_SIGN=1|-1
OAL_SYNSCAN_WIFI_AXIS2_SIGN=1|-1
```

Активні signs і джерело налаштування доступні в mount diagnostics як `transportAxis1Sign`, `transportAxis2Sign` та `transportPolaritySource`.


`synscan-wifi` підключається **безпосередньо до mount / EQDrive / SynScan Wi-Fi
адаптера**. SynScan Pro для цього не потрібен.

```text
OpenAstroLink -> UDP 11880 -> Wi-Fi адаптер mount/EQDrive -> motor controller
```

Payload — офіційний **Sky-Watcher Motor Controller Command Set**, тобто той
низькорівневий клас команд, який використовується й EQMOD-сумісними
контролерами.

```text
Backend:  synscan-wifi
Endpoint: auto
```

або:

```text
Backend:  synscan-wifi
Endpoint: 192.168.4.1:11880
```

`auto` надсилає лише read-only запит версії motor controller на UDP 11880. Крім
broadcast перевіряються типові AP gateway `192.168.4.1` та `192.168.0.1`, бо
деякі адаптери не відповідають на broadcast discovery.

Для прямого RA/DEC використовується консервативна sync-anchor модель: перед
GOTO треба один раз виконати Sync на відомій точці неба. Поки напрямки осей і
pier-side не пройшли HIL, одна native GOTO-команда обмежена невеликим кутом.

Джерело: Sky-Watcher Motor Controller Command Set,
<https://inter-static.skywatcher.com/downloads/skywatcher_motor_controller_command_set.pdf>.

## Через SynScan Pro — `synscan-app`

`synscan-app` — це higher-level compatibility path через **запущений SynScan
App/Pro** на телефоні або ПК.

```text
OpenAstroLink -> UDP 11881 -> SynScan Pro (телефон/ПК) -> mount
```

Endpoint — IP **пристрою, де запущено SynScan Pro**, а не IP Wi-Fi адаптера
монтування.

```text
Backend:  synscan-app
Endpoint: auto
```

або:

```text
Backend:  synscan-app
Endpoint: 192.168.0.100:11881
```

`auto` broadcast-ить безпечний `ServerVersion` на UDP 11881 і використовує
адресу SynScan App/Pro, що відповіла. Цей backend дає багатший telescope API:
position, asynchronous GOTO, tracking, sync, park/unpark та pulse guide, якщо
їх підтримують застосунок і mount.

Джерело: Sky-Watcher SynScan App Protocol,
<https://inter-static.skywatcher.com/downloads/synscan_app_protocol_20250930.pdf>.

## Порт 11882

TCP 11882 — це **SynScan Communication Protocol server, який експортує SynScan
App/Pro**. Це не direct Wi-Fi endpoint монтування. Починаючи з v0.2.10.18
`synscan-wifi` більше не означає TCP 11882: прямий Wi-Fi використовує UDP
11880.
