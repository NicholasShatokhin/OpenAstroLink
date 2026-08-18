# Матриця зрілості

| Модуль | Статус | Коментар |
|---|---|---|
| Simulated camera/mount/focuser | Робочий baseline | Для першого запуску і API тестів |
| OpenCV/UVC camera | Робочий baseline | Якість exposure control залежить від драйвера камери |
| Serial LX200 | Робочий baseline | GOTO/Sync/RA-DEC; park/tracking/pulse guide залежать від профілю пристрою |
| ASCOM Alpaca mount/focuser | Реалізовано | HTTP Device API; потрібен Alpaca server/device |
| GeminiAstro EAF | Compatibility profile додано | Alpaca або INDI transport; hardware validation ще не виконано; native USB protocol не заявляється |
| OAL client/server | Реалізовано | Mount/focuser/camera, solve, AF, guide, polar, sessions |
| QHY SDK | Опційний | Single-frame API; не перевірено на конкретній моделі у цьому середовищі |
| Canon/libgphoto2 | Опційний | Capture/download; керування bulb/shutter settings потребує model-specific config |
| INDI XML backend | Експериментальний | Стандартні property names; конкретний драйвер може відрізнятись |
| Star detector | Реалізовано | Adaptive threshold + connected components + HFR |
| Triangle solver | Прототип | Потребує hint/невеликого каталогу; не blind full-sky production solver |
| Neural solver | Інтерфейс | Runtime/model не включені |
| Autofocus stars/planet | Реалізовано | Coarse/fine + multiple-frame median |
| Bahtinov | Базова метрика | Не повний geometric spike displacement algorithm |
| Guiding | Базова closed-loop логіка | Потребує регулярних solved/guide measurements |
| Polar alignment | Реалізована математика | Потребує ≥2 якісних solve з RA rotation; краще 3–7 |
| Scheduler | Модель стану | Повна автоматична execution state machine ще не реалізована |
| Native OAL plug-ins | ABI + loader | Приклад є; адаптація plug-in devices до GUI registry — наступний крок |
