# Оптичні тракти та одночасна робота двох камер

Версія: 0.2.9

## Чому параметри телескопа зберігає core

Фокусна, апертура та sampling камери впливають на FOV hints для solver, перевірку plate scale, масштаб guiding, autofocus diagnostics та майбутні scheduler/safety checks. Тому вони зберігаються в node-side `TelescopeProfile`, а не лише в GUI.

## Головний оптичний тракт

Зберігаються тип оптики, апертура/діаметр головного дзеркала, центральне екранування за потреби, ефективна фокусна після reducer/Barlow, pixel size та розміри сенсора main camera.

```text
f-ratio = focalLengthMm / apertureMm
plate scale = 206.265 × pixelSizeUm / focalLengthMm  [arcsec/pixel]
```

## Гідуючий тракт

Окремо зберігаються назва guide scope, його апертура, фокусна та параметри guide camera. Для OAG ефективна guide focal length зазвичай відповідає головному оптичному шляху.

## Main + guide

Одночасно можна підключити дві камери. Registry розрізняє `role:"main"` і `role:"guide"`. Основна камера має resource `camera`, гідуюча — `camera.guide`, тому їхні exposure можуть виконуватись паралельно, якщо high-level workflow не блокує обидва ресурси.

Guide camera підключається через `POST /api/v1/devices/connect` із `"role":"guide"`, а guide exposure — через `POST /api/v1/cameras/guide/capture`.

## Поточна зрілість guiding

Dual-camera ownership і capture реалізовані. Повний production guider ще потребує calibration, star selection/centroid, subframes, dither, backlash compensation та star-loss recovery.
