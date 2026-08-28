# Plate solving — адаптивний режим для міського неба

Цей документ описує node-local pipeline v0.2.10.17 для міської засвітки, малих сенсорів і монтувань, де один кадр 10–15 с уже дає витягнуті зорі.

## Навіщо потрібен adaptive solve

Старий шлях лишається:

```text
один кадр -> solve last frame
```

Для складного міського поля рекомендований новий шлях:

```text
коротка експозиція
  -> оцінка якості
  -> solve, якщо кадр достатній
  -> інакше серія коротких кадрів
  -> видалення великомасштабного градієнта засвітки
  -> вирівнювання за зорями
  -> stack лише успішно зареєстрованих кадрів
  -> solve синтетичного solver frame
  -> розширення hinted search radius і повтор
```

Operation блокує `camera + solver`, підтримує cancel і завжди виконується в `openastrolink-node`, навіть для віддаленого GUI.

## Типова політика

Кнопка **Adaptive urban capture + solve** використовує базову експозицію та gain зі звичайних Capture-полів. Типові значення:

- binning 2x2;
- 3 спроби;
- проміжний stack: 3 короткі кадри;
- фінальний stack: 5 кадрів;
- максимум 3 с на один кадр;
- локальна ціль: 20 детектованих зір;
- background-gradient removal: увімкнено;
- registration: увімкнено;
- RA/Dec hint від mount: увімкнено за наявності монтування.

Остання спроба завжди запускає solver, навіть якщо локальний star-count нижчий за поріг.

## Binning і FOV

`CameraFrame` тепер зберігає `binX/binY`. ASTAP отримує FOV з урахуванням фактичного binning, тому кадр 2x2 більше не описується як поле приблизно вдвічі менше від реального.

## Hint від монтування

Якщо RA/Dec явно не задані і `useMountHint=true`, node бере поточні координати підключеного mount. Для типової триспробної схеми radius розширюється приблизно 5° -> 10° -> до налаштованого максимуму. Blind solve для міського режиму не є рекомендованим шляхом.

## Діагностика

У результаті operation є масив `attempts`: кількість захоплених/зареєстрованих кадрів, одиночна та сумарна експозиція, число зір, фон/шум, radius і повідомлення solver. `solverFrameId` вказує точний оброблений кадр, який отримав ASTAP; його можна відкрити через звичайний frame preview.

## REST

```http
POST /api/v1/solve/adaptive
Content-Type: application/json
```

Приклад:

```json
{
  "baseExposureSec": 1.5,
  "gain": 60,
  "binX": 2,
  "binY": 2,
  "maxAttempts": 3,
  "stackFrames": 3,
  "finalStackFrames": 5,
  "minStarsForSolve": 20,
  "maxSingleExposureSec": 3.0,
  "equalizeBackground": true,
  "registerFrames": true,
  "useMountHint": true,
  "searchRadiusDeg": 20
}
```

Відповідь — звичайний `202` operation resource.

## Перший тест

Для QHY5III462C у сильній засвітці почніть приблизно з 1–2 с, 2x2 і достатнього gain. Краще спочатку збільшувати gain та кількість кадрів у stack, а не одиночну експозицію вище межі, де через tracking/polar error зорі вже стають витягнутими.

Adaptive solve вирішує саме проблему отримання придатного solver-кадру. Він не замінює sidereal tracking, правильний optical profile, відповідну базу ASTAP і реальну HIL-перевірку на небі.

### Налаштування ASTAP для ноди

Ноду можна налаштувати без змінних середовища:

```text
openastrolink-node.exe --astap-executable "C:\\Program Files\\astap\\astap.exe" --astap-database "D:\\ASTAP\\D80" --astap-timeout-ms 60000
```

`--astap-database` має вказувати на каталог встановленої зоряної бази ASTAP. Adaptive pipeline також працює з налаштуванням через змінні середовища; ці параметри роблять конфігурацію віддаленої обсерваторії відтворюваною.

GUI має окрему **Adaptive base exposure** (типово 1.5 с), тому довга експозиція зі звичайного Capture не може випадково перетворити adaptive solve назад на 10–15-секундний кадр зі штрихами зір.


### Пам’ять DSLR та експозиція adaptive solve (v0.2.10.30)

Запитаний adaptive bin є **ефективним bin для solver**. Якщо камера ігнорує апаратний binning, node одразу зменшує кожен operational-кадр перед збереженням його для adaptive stack. Це не дозволяє накопичувати в пам’яті кілька повнорозмірних RGB-прев’ю DSLR. Capture-фаза також має обмежений wall-clock budget, а наступна коротка витримка вибирається за фоном/p99/saturation/кількістю зір; gain/ISO автоматично не змінюється.
