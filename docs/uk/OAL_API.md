# OAL API — v0.2.10.16

Канонічний документ: `../OAL_API.md`.

Node API забезпечує discovery, profile, device connect/disconnect, operations, mount/focuser/camera commands, solve, autofocus, guiding, polar alignment і sessions. У v0.2.10 camera connect має `role: main|guide`; guide capture доступний через `/cameras/guide/capture`. `/integrations/stellarium` керує TCP bridge.

Тривалі дії мають виконуватися як operation resources; REST/WS лишаються control plane, а великі science data мають переходити в окремий data plane.


## v0.2.10.16 adaptive solve

`POST /api/v1/solve/adaptive` запускає асинхронну `solver.adaptive` operation з locks `camera + solver`. Node сам робить короткі експозиції, registration/stack, background normalization та retry solver. Деталі: `docs/uk/PLATE_SOLVING.md`.
