# Мережева сумісність із SynScan

OpenAstroLink має два network compatibility path через запущений Sky-Watcher SynScan / SynScan Pro. Вони окремі від прямих нативних драйверів `oal.skywatcher` та `oal.eqdrive`.

## Рекомендований: SynScan App Protocol (`synscan-app`)

Багатший шлях використовує офіційний **SynScan App Protocol**, типовий UDP port 11881. Протокол близький за можливостями до ASCOM `ITelescopeV3` і дає position, asynchronous GOTO, tracking, sync, park/unpark та pulse guiding.

Endpoint — це IP **телефона або ПК, на якому запущено SynScan Pro**, а не автоматично IP Wi-Fi адаптера монтування.

Приклад:

```text
Backend: synscan-app
Endpoint: 192.168.4.2:11881
```

Якщо SynScan Pro і OpenAstroLink працюють на одному Windows ПК, можна використовувати `127.0.0.1:11881`.

Джерело: Sky-Watcher SynScan App Protocol, <https://inter-static.skywatcher.com/downloads/synscan_app_protocol_20250930.pdf>.

## Serial-протокол поверх TCP (`synscan-wifi`)

Другий, вужчий backend під'єднується до TCP-сервера **SynScan Communication Protocol** у SynScan Pro, типовий port 11882, і повторно використовує наявний SynScan serial RA/Dec codec OAL.

Приклад:

```text
Backend: synscan-wifi
Endpoint: 192.168.4.2:11882
```

Цей варіант корисний для compatibility/testing, але 11881 App Protocol бажаніший, коли доступні обидва, бо має багатший telescope API.

## Чим це не є

Ці backends керують mount **через застосунок SynScan**. Це не direct UDP-to-mount Motor Controller transport. Для прямого контролера слід використовувати відповідний native OAL driver, якщо він є.

## Типова топологія

```text
OpenAstroLink node ---- Wi-Fi/LAN ---- SynScan Pro (телефон/ПК)
                                      |
                                      +---- transport mount ---- mount
```

Фактичні порти видно в налаштуваннях SynScan Pro; endpoint OAL можна змінити відповідно.
