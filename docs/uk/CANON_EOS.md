# Нативний Canon EOS — v0.2.10

Канонічний документ: `../CANON_EOS.md`.

`oal.canon` є native OAL camera driver через USB/PTP transport libgphoto2, а не INDI wrapper. Він підтримує discovery, exposure/Bulb, RAW/JPEG, оригінальні CR2/CR3 artifacts, preview та cooperative cancellation. Конкретні моделі Canon мають пройти HIL на RPi.

## v0.2.10: вибір transport

Native Canon тепер кросплатформний OAL driver із build-time вибором transport:

- `OAS_CANON_TRANSPORT=AUTO` → **EDSDK на Windows**, **GPHOTO2 на Linux**;
- `OAS_CANON_TRANSPORT=EDSDK` → явно Canon EDSDK;
- `OAS_CANON_TRANSPORT=GPHOTO2` → явно libgphoto2.

З боку OAL driver id завжди `oal.canon`, тому GUI/core/session не залежать від low-level transport.

Windows EDSDK implementation перераховує EOS, відкриває session, зберігає оригінальний файл і публікує thumbnail EDSDK як operational preview. Для astronomy exposure використовується Bulb, якщо камера/режим це дозволяє; cancellation завершує Bulb best-effort.

Потрібні CMake variables:

```text
CANON_EDSDK_INCLUDE_DIR
CANON_EDSDK_LIBRARY
CANON_EDSDK_RUNTIME_DIR
```

Цей шлях ще потребує hardware HIL на конкретній EOS та актуальній EDSDK runtime.

На Linux за замовчуванням лишається native OAL через libgphoto2/PTP; libgphoto2 тут лише hardware-access layer, а не INDI/ASCOM backend.
