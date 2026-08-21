# Нативний Canon EOS — v0.2.9

Канонічний документ: `../CANON_EOS.md`.

`oal.canon` є native OAL camera driver через USB/PTP transport libgphoto2, а не INDI wrapper. Він підтримує discovery, exposure/Bulb, RAW/JPEG, оригінальні CR2/CR3 artifacts, preview та cooperative cancellation. Конкретні моделі Canon мають пройти HIL на RPi.
