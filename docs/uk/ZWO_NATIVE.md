# Нативні драйвери ZWO ASI та ZWO EAF


> **Поточний реліз:** v0.2.10.50. Native ZWO ASI/EAF лишаються default OAL drivers; ARM64 vendor libraries пройшли повний Raspberry Pi cross-build.

Версія: 0.2.10

## Призначення

ZWO підтримується як first-class native OpenAstroLink hardware family. Основний шлях: `ZWO SDK → native OAL ABI v2 → OpenAstroLink core`, а не `ZWO → INDI → OAL`.

## oal.zwo.asi

Драйвер знаходить усі підключені ASI-камери й створює окремий OAL device для кожної. Це дозволяє одночасно мати, наприклад, головну охолоджувану ASI і малу ASI для гіду, або QHY/Canon як main і ASI як guide.

Реалізовано identity, геометрію сенсора, pixel size/bit depth/color, binning/formats, gain/offset ranges, ROI, single exposure, cancel та native frame publication. ASI SDK також має video capture; повний OAL streaming/ring-buffer profile ще потребує production hardening.

CMake:

```text
OAS_ENABLE_NATIVE_ZWO_ASI=ON
ZWO_ASI_ROOT=/path/to/sdk
```

або точні `ZWO_ASI_INCLUDE_DIR` і `ZWO_ASI_LIBRARY`.

## oal.zwo.eaf

Реалізовано connect/disconnect, position, absolute/relative move, halt/cancel, moving state, temperature за наявності, max step, reverse та backlash capabilities.

```text
OAS_ENABLE_NATIVE_ZWO_EAF=ON
ZWO_EAF_ROOT=/path/to/sdk
```

## Потрібен HIL

До production status потрібні тести на реальних ASI/EAF на Raspberry Pi: серії exposure, abort, ROI/binning, дві ASI одночасно, reconnect, EAF move/halt/limits/temp та повторюваність autofocus.
