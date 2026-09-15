# Observable Sky Region — доступна ділянка неба

Стан: **v0.2.10.60 source/static-qualified; fresh Windows build і HIL ще потрібні.**

## Призначення

Observable Sky Region описує не геометричний горизонт, а ту частину неба, яку оптична система реально бачить без перешкод із конкретного місця. Це особливо корисно на балконі, біля вікна, у дворі, на даху або поруч із будинками й деревами.

Це навмисно **не hard mechanical safety model**.

- **Observable Sky Region** — visibility/planning constraint.
- **Raw-axis mechanical guard** — жорстке механічне/колізійне обмеження.
- **Polar Motion Limits** — окрема optional safe region для рухів алгоритму Polar Alignment.

Ручний джойстик не блокується Observable Sky Region, щоб оператор завжди міг виконати калібрування або вручну вивести телескоп із будь-якої позиції.

## Швидке калібрування двома кутами

У v0.2.10.60 реалізований простий rectangle workflow:

1. вручну навести телескоп у один край доступного неба;
2. натиснути **Capture first visible corner from current pointing**;
3. перейти джойстиком у протилежний край;
4. натиснути **Capture opposite corner and save rectangle**.

Обидві точки переводяться у локальні Alt/Az. Межі по висоті беруться як min/max. По азимуту вибирається коротша дуга; якщо область переходить через північ, вона зберігається як `minAzDeg > maxAzDeg`.

Цей quick mode розрахований на відносно компактний прямокутний отвір. Довільний polygon, кілька вікон і окремі obstruction masks залишаються подальшим розширенням.

## Поля профілю

`TelescopeProfile.observableSky`:

- `enabled`;
- `minAzDeg`, `maxAzDeg`;
- `minAltDeg`, `maxAltDeg`;
- `rejectAutomatedGoto`;
- `schedulerEligibility`;
- `recheckSeconds`.

Область зберігається разом із telescope profile і однаково доступна local/remote GUI.

## Sky Map

Коли region увімкнена, її межі малюються безпосередньо на Sky Map. Область, що перетинає Az=0°/північ, показується як одна неперервна локальна зона.

## Політика automated GOTO

Якщо `rejectAutomatedGoto` увімкнено, synchronous та asynchronous automated GOTO за межі області відхиляються ще до початку руху.

На manual joystick ця політика не поширюється.

## Scheduler

Якщо `schedulerEligibility` увімкнено:

- перед стартом block OAL перевіряє, чи його поточна ціль усередині region;
- для mosaic перевіряється центр поточного tile;
- якщо ціль виходить із зони під час FITS exposure або planetary SER, активний запис не обривається;
- на наступній безпечній межі acquisition поточний block можна відкласти й перейти до іншого due/visible block;
- частковий progress відкладеного block зберігається, доки працює поточний node process;
- якщо доступної due-цілі немає, Scheduler переходить у `waiting-observable-sky` і періодично перевіряє plan;
- майбутній block не запускається раніше `startAtUtc` лише тому, що зараз він видимий.

### Обмеження crash resume

Поточний Beta Scheduler має лінійний persisted next-block cursor, а не повний журнал завершення блоків після runtime reorder. Тому v0.2.10.60 зберігає out-of-order partial progress у процесі та записує консервативний resume hint, щоб незавершений відкладений block не був тихо пропущений. Після crash уже завершений пізніший block іноді може повторитися. Durable per-block journal — задача OAL 1.0.

## API

- `POST /api/v1/observable-sky/corner/1`
- `POST /api/v1/observable-sky/corner/2`
- `POST /api/v1/observable-sky/clear`
- persisted policy передається через звичайний `GET/POST /api/v1/profile`.

## HIL acceptance

1. записати перший край доступної ділянки;
2. записати протилежний край;
3. перевірити overlay на Sky Map;
4. перевірити reject automated GOTO поза зоною;
5. перевірити, що manual joystick поза зоною лишається доступним;
6. створити Scheduler plan щонайменше з двома цілями;
7. дочекатися виходу поточної цілі за межу під час FITS/SER;
8. переконатися, що поточний exposure/SER завершується нормально;
9. перевірити перехід до іншої доступної цілі та подальше повернення до deferred block.
