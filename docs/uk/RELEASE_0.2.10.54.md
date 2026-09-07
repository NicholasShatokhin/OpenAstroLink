# OpenAstroLink / OpenAstroSuite v0.2.10.54

## HIL-hardening від 2026-09-06

Цей реліз базується на реальних тестах QHY5III462C + Gemini focuser + direct SynScan/EQDrive Wi-Fi. Нова геометрія монтування не вводиться.

### Що вже підтверджено HIL

- GOTO у довільну/free-point точку Sky Map фізично рухає монтування у вибрану частину неба.
- Координати catalogue/free-point target із Sky Map передаються у Scheduler.
- Direct-MC v9 GOTO та Abort працюють; geometry v9 frozen і не змінювалася.
- Native QHY Live View багаторазово запускається/зупиняється; still FITS успішно зберігаються.
- Gemini focuser виконує absolute move, cancellation recovery та віддає status/temperature.
- При скасуванні scene autofocus початкова позиція фокусера відновлюється.
- Базова сумісність SER підтверджена відкриттям OAL SER у AutoStakkert.

### Надійність ручного руху по Wi-Fi

Лог показав, що press/release доходять до node, але фізичний UDP/11880 рух інколи не реагував на press або продовжувався після release. Direct Wi-Fi backend тепер:

- перевіряє адресу **і порт** джерела UDP-відповіді;
- використовує Motor Controller instant stop (`L`) для release/abort;
- повторює manual start/stop transaction;
- перевіряє фактичний running/stopped post-condition через axis status;
- прибирає старий 2.5-секундний stop-and-wait перед кожним press;
- не зупиняє зайвий уже нерухомий другий axis під час single-axis press;
- при непідтвердженому start fail-safe зупиняє обидві осі.

Інверсія Axis 1/2 у Mount tab тепер задається checkbox, який показує поточний profile state. Це змінює лише profile sign, а не HIL-кваліфіковані v9 equations.

### Sky Map → Mount target

Вибір catalogue або free-point target на Sky Map також копіює J2000 RA/DEC у synchronized target fields Mount tab. Сам вибір не запускає slew автоматично.

### Scene autofocus

HIL виявив дві незалежні проблеми: sparse structured target змушував meter піднімати експозицію аж до 10 с, а global whole-frame edge score був настільки шумним, що алгоритм міг пересунути фокус через випадкову ~1% різницю. Тепер scene AF:

- metering використовує bright tail P99/P99.5, а не вимогу високих global P50/P95;
- обмежує metering 2 секундами та чотирма спробами;
- тримає одну fixed exposure під час усього sweep;
- рахує high-frequency metric по tiles і агрегує найінформативніші ділянки;
- використовує максимум два frames/point для scene mode;
- не залишає вже добрий starting focus без improvement, що перевищує 2%/measured-spread uncertainty;
- біля локального максимуму робить компактний ±2 fine-step refinement замість широкого fine sweep;
- після move перевіряє peak повторно і повертає/лишає original focus, якщо improvement не повторився.

Range лишається справжньою межею пошуку: `1600` означає лише ±800 steps. Такий range фізично не може знайти фокус, який лежить на кілька тисяч steps далі.

### Still auto-exposure

Надана серія QHY FITS показала двоточковий limit cycle приблизно між ~0.64 с та ~1.43 с: old controller намагався підняти global median, після чого різко реагував на clipping. Тепер controller:

- для sparse bright scene контролює P99.5 замість global median;
- застосовує smooth damping і log-space bisection після crossing target;
- допускає невелику (<~0.5%) частку saturated specular/star pixels, якщо bright-tail level здоровий, замість жорсткого порога;
- lock-ить exposure у цільовій зоні та unlock-ить лише після стійкої зміни сцени;
- повністю reset-ить history при зміні gain.

### Preview reliability

- Bright-target detector працює по чистому camera image, а не після малювання crosshair/grid; це прибирає false target `x=17, y=17` від UI overlay.
- State refresh для `live-*` frame використовує `latest`, тому high-FPS preview більше не має benign cache-eviction race.

## Що ще перевірити HIL

- багато швидких Wi-Fi press/release та change-direction;
- scene AF біля справжнього focus і з навмисним, але in-range defocus;
- той самий sparse daylight target для auto-exposure — має lock-нутися без oscillation;
- star-field autofocus/HFR окремо вночі;
- physical qualification macOS і Raspberry Pi 5.
