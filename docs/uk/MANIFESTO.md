# Маніфест OpenAstroLink

OpenAstroLink створюється для того, щоб астрономічне обладнання було **відкритим для керування, автоматизації та інтеграції** без перетворення одного GUI, однієї ОС, одного vendor SDK або compatibility daemon на постійний центр обсерваторії.

## Принципи

### 1. Обладнання належить node

Процес, найближчий до hardware, володіє device sessions. GUI, scripts та remote applications — клієнти. Закриття або заміна GUI не повинні змінювати ownership монтування, камери чи фокусера.

### 2. Native first, compatibility welcome

Direct vendor SDK і hardware protocols є first-class OAL backends там, де дають потрібні capabilities чи latency. INDI, ASCOM/Alpaca, LX200 та інші ecosystems залишаються interoperability paths. Native-first — архітектурний вибір, а не відмова від існуючих спільнот.

### 3. Capabilities замість припущень

Software має питати device, що він уміє. Camera, mount, focuser або майбутній observatory device повинні публікувати identity та capabilities, щоб clients не залежали від vendor-name conditionals.

### 4. Астрономічні дії є operations

Реальна observing action має lifecycle. Operations можуть queue-итися, виконуватися, показувати progress, володіти resources, падати та cancel-итися. Одна модель має працювати і для interactive control, і для automation.

### 5. Recording важливіший за preview

Science data path не повинен залежати від GUI rendering чи network congestion. Preview може адаптуватися або drop-ати кадри. Authoritative FITS/RAW/SER acquisition має зберігати власний timing і provenance.

### 6. Remote control — не додаткова функція

Один node має обслуговувати local GUI, remote workstation, script, mobile client або іншу astronomy application через documented protocol surfaces.

### 7. Safety — окремий layer, а не прихована geometry

Mechanical guards, sky-safety policy та operator limits мають бути явними. Driver geometry не повинна таємно змінюватися, щоб імітувати safety rule. HIL-qualified geometry frozen до появи нових доказів.

### 8. Failure behaviour є частиною interface

Cancel, reconnect, dropped preview, device disappearance, partial network failure та restart recovery — не «дрібні edge cases». Це поведінка обсерваторії, яку треба тестувати.

### 9. Evidence важливіший за впевненість

Feature не називається hardware-qualified лише тому, що код виглядає правильним. Build qualification, simulation/regression evidence та real HIL evidence фіксуються окремо.

### 10. OpenAstroSuite — reference client, не gatekeeper

Сторонні програми повинні використовувати OAL без embedding чи копіювання OpenAstroSuite GUI. Protocol, driver ABI та integration documentation є публічними interfaces проєкту.

### 11. Дані та workflow належать користувачу

Science frames, capture metadata, calibration provenance та observing plans мають залишатися зрозумілими й exportable. Система повинна віддавати перевагу open formats і reproducible metadata.

### 12. До unattended operation треба доростати свідомо

Supervised Beta та unattended 1.0 — різні safety claims. Authentication, audit, weather/roof/power policy, durable recovery та emergency behaviour треба завершити до того, як unattended use буде позиціонуватися як production-ready.

## Обіцянка contributors і users

OpenAstroLink документуватиме qualification boundaries, зберігатиме HIL-підтверджені invariants, підтримуватиме English canonical documentation з Ukrainian mirrors і надаватиме перевагу явним interoperable interfaces замість hidden application coupling.
