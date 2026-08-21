# OAL API — v0.2.10

Канонічний документ: `../OAL_API.md`.

Node API забезпечує discovery, profile, device connect/disconnect, operations, mount/focuser/camera commands, solve, autofocus, guiding, polar alignment і sessions. У v0.2.10 camera connect має `role: main|guide`; guide capture доступний через `/cameras/guide/capture`. `/integrations/stellarium` керує TCP bridge.

Тривалі дії мають виконуватися як operation resources; REST/WS лишаються control plane, а великі science data мають переходити в окремий data plane.
