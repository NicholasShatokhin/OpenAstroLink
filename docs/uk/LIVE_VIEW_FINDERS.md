# Live View, Scene Autofocus та юстування шукача

OpenAstroLink v0.2.10.35 додає operational-preview workflow для налаштування телескопа, захоплення цілі, юстування шукача, кадрування Місяця/планет і денного фокусування.

## Live View operation

`POST /api/v1/cameras/main/live-view` запускає cancellable operation `camera.live-view`. Вона утримує ресурс головної камери до скасування, циклічно отримує короткі кадри й публікує їх через звичайний WebSocket `frameReady` + HTTP preview path.

Live-кадри є **тільки preview**:

- примусово використовується `saveRaw=false`;
- QHY/ASI-подібні кадри не пишуться у FITS;
- вони оновлюють RAM preview-cache та GUI;
- типові GUI-параметри: 50 ms, gain 100, 2x2 bin, target 5 fps.

Фактичний FPS обмежують витримка, readout камери, PNG encoding, мережа і binning.

### Безпека Canon DSLR

У v0.2.10.33 Live View для native Canon EDSDK still-capture path навмисно відхиляється. Серійні CR2 на кількох FPS постійно спрацьовували б механічним затвором і не є прийнятною заміною EVF. Наступний Canon-етап — окремий EDSDK EVF transport.

## Вкладка Live / Finder

Вкладка **Live / Finder** має:

- exposure, gain, binning і target FPS;
- auto-stretch preview;
- перехрестя по центру сенсора;
- пошук найяскравішої області та приблизне зміщення від центра в пікселях;
- Scene autofocus із поточними Live View параметрами;
- п'ятикроковий Finder Alignment wizard.

Пошук яскравої області — це інструмент acquisition, а не астрометричний centroid. Використовується block-energy пошук на зменшеному кадрі, стійкіший до одиничних hot pixels, ніж пошук одного найяскравішого пікселя.

## Scene autofocus

`AutofocusMode::Scene` фокусується по структурованому незоряному зображенню через gradient-energy метрику. Типові цілі:

- далека антена або щогла при денному налаштуванні;
- деталі поверхні Місяця;
- диск планети, коли зоряних PSF немає;
- інші достатньо текстуровані оптичні об'єкти.

Exposure і gain автофокуса тепер є явними параметрами request, а не жорстко 50 ms / gain 0.

Star autofocus лишається PSF-орієнтованим і тепер вимагає щонайменше `minStars` придатних зір на focus-frame. Якщо критерій не виконується, operation повертає зрозуміле `No suitable stars detected`, а не шумовий «кращий фокус».


## Опційний camera-neutral debayer (v0.2.10.35)

Live View може опційно виконувати software debayer одноканального CFA-preview. Це **лише preview**: RAW-пікселі сенсора та science FITS/RAW ніколи не змінюються.

- `AUTO` бере `bayerPattern` із metadata активного native-драйвера.
- QHY визначає sequence через SDK `CAM_COLOR`, тому підтримуються різні кольорові моделі QHY без hardcode конкретної камери.
- ZWO ASI використовує `ASI_CAMERA_INFO::BayerPattern`.
- RGGB, BGGR, GRBG і GBRG можна вибрати вручну як vendor-neutral fallback для інших RAW/Bayer-камер.
- Для debayer потрібна рідна 1x1 CFA-решітка, тому при його увімкненні Live View примусово використовує 1x1.
- Якщо камера вже повертає RGB, preview проходить без повторного debayer.

Пересвічений чи недоекспонований кадр залишається валідним кадром камери. GUI може показати warning якості експозиції, але це не transport failure і не disconnect.

## Візуальний feedback під час autofocus (v0.2.10.35)

Autofocus публікує один preview-кадр для кожної sampled позиції фокусера у головну панель камери. Ці operational previews не замінюють останній science-frame. На Focus також є coarse/fine jog кнопки та STOP.

Scene autofocus зберігає сильний coarse-пік, якщо fine-pass виявився слабшим, відкидає майже пласкі криві й може розширити scan, якщо максимум потрапив на край.

## Workflow юстування шукача

1. Наведіть телескоп на далекий контрастний наземний об'єкт і запустіть Live View.
2. Зупиніть Live View і запустіть Scene autofocus. Після завершення знову ввімкніть Live View.
3. Рухом телескопа поставте впізнавану деталь точно під camera crosshair.
4. Телескоп більше не рухайте. Регулюйте тільки гвинти шукача, доки та сама деталь не стане в центрі його перехрестя.
5. Перевірте: трохи відведіть телескоп і поверніться на ціль. Уночі уточніть по яскравій зорі та зробіть Star autofocus на нескінченності.

Ніколи не спрямовуйте телескоп або камеру на Сонце без відповідного фільтра перед апертурою.

## Подальший acquisition

Bright-region detector v0.2.10.33 є основою для майбутнього `Acquire bright target`. Automatic mount centering та spiral search навмисно не вмикаються, доки native mount coordinate/sign/tracking model не завершить HIL-кваліфікацію.

## Science-файли та remote GUI

Кадри Live View і autofocus навмисно залишаються preview-only. Звичайний користувацький **Capture** може вимагати збереження science-файлу. У v0.2.10.34 remote GUI передає `saveRaw` та необов’язковий `savePath` на node; тому native host-frame камери на кшталт QHY зберігаються node у FITS у science spool, а Canon і далі зберігає оригінальний RAW з камери.
