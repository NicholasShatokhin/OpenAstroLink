# Інтеграція Stellarium

> Поточний synchronized snapshot: **v0.2.10.57 (2026-09-07)**. Див. `STATUS.md` та `RELEASE_0.2.10.57.md` щодо current qualification boundaries.


> **Поточний реліз:** v0.2.10.53. Стандартний Telescope Control bridge лишається шляхом mount-position/GOTO; v0.2.10.53 додатково вміє передавати camera footprint через optional Stellarium Remote Control.

OpenAstroLink навмисно має дві окремі інтеграції зі Stellarium.

## 1. Telescope Control TCP bridge

Це існуючий mount bridge. Він:

- передає active OAL mount RA/Dec у Stellarium;
- декодує Stellarium GOTO і передає його через active OAL mount/controller path;
- може працювати на Raspberry Pi node при remote GUI;
- default TCP port — `10000`.

```bash
openastrolink-node --stellarium-port 10000
```

REST:

```text
GET  /api/v1/integrations/stellarium
POST /api/v1/integrations/stellarium
```

Стандартний telescope protocol **не** переносить camera FOV rectangles, focusers, autofocus, polar alignment, sessions чи повну OAL observatory state.

## 2. Remote Control footprint export — v0.2.10.53

Sky Map в OpenAstroSuite може напряму звертатися до optional **Remote Control** HTTP plugin Stellarium (default `http://127.0.0.1:8090`). Це GUI-local path, тому він працює і коли OpenAstroSuite є thin client до remote OAL node, а Stellarium запущений на operator PC.

Можна передати:

- останню solved рамку main camera;
- predicted main-camera frame;
- predicted guide-camera frame;
- mosaic envelope.

Implementation contract:

1. знайти registered `SpecialMarkersMgr` rectangular-FOV property IDs через `GET /api/stelproperty/list`;
2. задати width, height, rotation і visibility через `POST /api/stelproperty/set`;
3. центрувати Stellarium на J2000 center через `POST /api/main/view`;
4. підлаштувати viewport FOV через `POST /api/main/fov`.

Stock `SpecialMarkersMgr` має одну rectangular frame, тому **Send frame** замінює попередній exported OAL footprint. Кілька одночасно persistent OAL rectangles потребують dedicated Stellarium plugin і лишаються later scope.

Remote Control plugin має бути увімкнений у Stellarium. Якщо він недоступний, export завершується нормальною помилкою й не впливає на TCP telescope bridge.

## Safety і coordinate policy

Жоден із цих шляхів не вводить нову mount geometry. Telescope Control GOTO проходить через active OAL mount і frozen v9 Core geometry/safety policy. Footprint export — лише display і ніколи не рухає монтування.

TCP position packet нормалізується у J2000. Sky-frame export також використовує J2000 center. Raw-axis EQDrive як і раніше потребує valid OAL sky model (Home restoration або Sync).

Не виставляйте TCP/HTTP integrations напряму в public Internet; використовуйте trusted LAN/VPN та OAL security plan.
