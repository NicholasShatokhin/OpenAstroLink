# Нативний Canon EOS — v0.2.10.5

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

## v0.2.10.26 — автоматичний EDSDK hot-plug та ISO через gain

- Callback EDSDK `camera-added` тепер надсилає OAL-подію `device.discoveryHint`; застосунок перетворює її на асинхронний перепошук лише драйвера `oal.canon`. На node вже наявний шлях `nativeDiscoveryCompleted` після цього відновлює збережене auto-connect підключення Canon без натискання глобального Refresh.
- OAL `gain` у Windows EDSDK-транспорті тепер означає **ISO**, як і в Linux/libgphoto2-транспорті Canon. `gain=0` залишає поточне ISO камери; додатне значення мапиться на найближче ISO, яке камера рекламує через `kEdsPropID_ISOSpeed`. Якщо запитане ISO неможливо застосувати, capture завершується явною помилкою замість тихого використання старого ISO.
- Оригінальний CR2/CR3 залишається science-артефактом. В оперативний `CameraFrame` зараз потрапляє EDSDK/embedded JPEG, який використовують GUI, autofocus і plate solving. Для перевіреного CR2 з EOS 550D це 5184×3456: повна просторова роздільність, але не 14-бітні RAW-пікселі.
- Типовий Windows-каталог оригіналів: `%USERPROFILE%\Pictures\OpenAstroLink\Canon`; шлях також видно в логах як `original=...`.

## v0.2.10.25 — hot-plug EOS 550D, RAW-preview та сумісний Bulb

Наступний Windows HIL на Canon EOS 550D встановив ще три важливі факти:

- Камера може згенерувати EDSDK camera-added callback раніше, ніж стане видимою через `EdsGetCameraList()`. Явна кнопка **Refresh all native devices** тепер дає pending camera-added події обмежене вікно ~1.2 с для settle/retry і не скидає подію, доки камера реально не стала enumerable. Якщо після цього список все ще порожній, лишається фінальний fallback з hard reload Canon driver/EDSDK, але тільки після явного Refresh. Періодичного Canon scan немає.
- Експозиції 1–30 с фізично виконувалися, а оригінальні файли успішно передавалися, але ця комбінація EOS/RAW не повертала дані через `EdsDownloadThumbnail()`. Відсутність thumbnail більше не робить успішний capture помилкою. OAL спочатку пробує EDSDK thumbnail, потім JPEG-оригінал, а для CR2 витягає найбільший декодований вбудований JPEG-preview із самого RAW, не змінюючи science-grade оригінал. Джерело preview пишеться в лог і metadata.
- `EdsSendCommand(BulbStart)` на EOS 550D повернув EDSDK `0x60` (`EDS_ERR_INVALID_PARAMETER`). Для довгих витримок основним шляхом тепер є `Tv=Bulb` + утримання `PressShutterButton(Completely_NonAF)` + `ShutterButton_OFF`. `BulbStart/BulbEnd` лишився лише compatibility fallback для камер, які не підтримують shutter-hold шлях.

Короткі витримки до 30 с, як і раніше, виконуються без AF: requested exposure мапиться на найближче доступне `kEdsPropID_Tv`, а спуск робиться через `Completely_NonAF`. У metadata зберігаються requested та actual exposure.

У Devices UI тепер навмисно розділено:

- **Apply port & rediscover selected serial driver** — тільки Gemini/Sky-Watcher/EQDrive.
- **Refresh all native devices (USB / serial)** — vendor-neutral discovery для Canon, QHY, ZWO та serial hardware.

Рекомендований HIL EOS 550D:

1. Запусти node без EOS, підключи камеру, дочекайся `camera-added event received` і один раз натисни **Refresh all native devices (USB / serial)**.
2. Перевір `Canon EDSDK discovery scan sees 1 camera(s)` і появу `native:oal.canon/...` без restart node.
3. Зроби 1 с і 10 с. Оригінальний CR2/JPEG має зберегтися, а preview — з'явитися в GUI. У node має бути `Canon preview source=...`.
4. Зроби 45 с. У Bulb-capable Manual mode для EOS 550D очікується `Canon Bulb transport=held-non-af-shutter`.
5. Скасуй одну довгу експозицію та перевір, що затвор одразу відпустився.
6. Зроби серію з 20 коротких кадрів для кваліфікації стабільності transfer/event pipeline.

