# Raspberry Pi observatory node — v0.2.9

Канонічний документ: `../RPI_NODE.md`.

`openastrolink-node` запускається headless через systemd, володіє пристроями та виконує autofocus, solve, polar alignment, guiding і sessions. GUI на самому RPi та GUI на іншому комп'ютері є клієнтами одного core.

Stellarium bridge можна запускати з `--stellarium-port 10000` або settings/API. INDI compatibility можна додати без зміни native OAL конфігурації.
