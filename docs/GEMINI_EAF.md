# GeminiAstro EAF / Automatic Astro Focuser Pro

## Статус підтримки

OpenAstroSuite має окремий backend-профіль `gemini-eaf` для фокусера GeminiAstro.
На цьому етапі він **не реалізує неперевірений vendor-specific USB/serial protocol**. Замість цього
профіль використовує два transport-и, які виробник заявляє як підтримувані:

1. **ASCOM → ASCOM Remote / Alpaca** — рекомендований шлях для Windows.
2. **INDI** — для Linux/Raspberry Pi та інших deployment-ів, де доступний відповідний INDI driver.

Це дає окрему identity на рівні OpenAstroSuite (`backendName = gemini-eaf`), але не прив'язує OAL
до неофіційно реконструйованого протоколу конкретної версії firmware.

Офіційні сторінки виробника:

- https://geminiastro.cc/
- https://geminiastro.cc/products/eaf/
- https://geminiastro.cc/downloads/

## Налаштування

У вкладці **Devices → Focuser** вибрати `gemini-eaf`.

### Варіант A — ASCOM/Alpaca

Endpoint:

```text
alpaca:http://127.0.0.1:11111/api/v1/focuser/0
```

Префікс `alpaca:` рекомендований, але звичайний `http://` / `https://` URL також приймається.
На Windows практична схема така:

```text
Gemini ASCOM driver → ASCOM Remote → Alpaca HTTP → OpenAstroSuite gemini-eaf
```

### Варіант B — INDI

Збирати OpenAstroSuite з:

```bash
cmake -S . -B build -DOAS_ENABLE_INDI=ON
cmake --build build --config Release
```

Endpoint:

```text
indi:127.0.0.1:7624/Exact Device Name
```

Явний префікс `indi:` рекомендований. Якщо INDI backend не включений під час build, програма
повертає зрозумілу помилку замість мовчазного fallback.

## Що проходить через профіль зараз

Профіль делегує стандартному focuser transport:

- connect / disconnect;
- current absolute position;
- absolute move;
- relative move;
- halt;
- moving state — коли transport його надає;
- temperature — коли transport його надає.

Для Alpaca `moving` і `temperature` вже читаються стандартним backend-ом. Вбудований мінімальний
INDI backend зараз гарантує тільки базовий standard-property path для position/move/halt; його
telemetry буде розширена разом із P0 capabilities/discovery.

## Hardware validation checklist

Перед позначенням GeminiAstro EAF як hardware-validated треба пройти окремо для Alpaca та INDI:

1. Connect/disconnect 20 разів без зависань.
2. Прочитати позицію після старту і після power-cycle.
3. Relative move `+100`, перевірити зміну позиції.
4. Relative move `-100`, перевірити повернення.
5. Absolute move у безпечну середню позицію.
6. Під час довшого move викликати `halt` і перевірити фактичну зупинку.
7. Перевірити `moving` під час руху та після завершення.
8. Якщо підключений temperature probe — звірити temperature telemetry з vendor application.
9. Перевірити поведінку на lower/upper limits і відсутність wrap/overflow.
10. Провести короткий autofocus scan, не допускаючи виходу за механічні межі.
11. Імітувати втрату USB/network під час move; переконатися, що стан стає error/unknown, а не
    неправдиво `succeeded`.
12. Зафіксувати версії Gemini firmware/driver, OS, ASCOM/INDI stack і endpoint у validation log.

## Native USB/serial driver — наступний окремий етап

Прямий native driver має з'явитися лише після отримання достовірної command specification або
hardware capture для **поточної** firmware. План:

1. Зафіксувати точну модель, firmware і USB bridge/VID/PID на реальному пристрої.
2. Попросити у виробника command/protocol specification; це пріоритетніше reverse engineering.
3. Якщо документації немає — записати traffic офіційного Console/ASCOM driver для повної матриці
   read/move/halt/limits/temperature/error cases.
4. Побудувати simulator із записаними golden transcripts.
5. Реалізувати `GeminiEafSerialFocuser` тільки після того, як protocol matrix не має невідомих
   safety-critical команд.
6. Прогнати ті самі conformance tests, що й для Alpaca/INDI профілю.
7. Лише після hardware-in-the-loop regression додати native transport у список production-capable.

Це узгоджується з принципом OAL: capability має бути виявленою властивістю конкретного драйвера,
а не припущенням клієнта про бренд або модель.
