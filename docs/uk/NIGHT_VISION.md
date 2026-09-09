# Night Vision і Strict Night Mode

Поточний source checkpoint: **OpenAstroLink / OpenAstroSuite v0.2.10.59**.

У desktop OpenAstroSuite є три режими відображення в **View → Night vision mode**:

- **Normal** — звичайна системна палітра та наявні семантичні кольори.
- **Night Vision** — стандартний Qt Widgets інтерфейс, меню, діалоги, поля, кнопки, tooltip-и та нейтральні графічні поверхні переводяться в малояскраву чорно-червону палітру. Пікселі камери не перефарбовуються. Семантичні custom-графіки можуть зберігати власні кольори, якщо це корисно оператору.
- **Strict Night Mode** — включає Night Vision і додатково переводить custom-графіку GUI в red-only: Sky Map, histogram, astrometry/star overlays, reticle/grid, target markers, status accents та інші hard-coded кольори. Це режим для максимального збереження темнової адаптації.

`Ctrl+Shift+N` циклічно перемикає **Normal → Night Vision → Strict Night Mode → Normal**. Режим зберігається через `QSettings` і застосовується ще до діалогу вибору observatory core, щоб після нічної сесії при запуску не з'являлося яскраве біле вікно.

## Live View

Night/Strict **не змінюють пікселі камери автоматично**.

У вкладці Live/Finder є окремий persistent checkbox:

> **Black→red palette for monochrome/raw preview (display only; Debayer must be off)**

Якщо він увімкнений, справді монохромний preview (у тому числі одноканальний RAW/CFA без debayer) показується шкалою чорний→червоний. Справжнє кольорове зображення залишається кольоровим. Якщо для Main увімкнути debayer, black→red transform не застосовується.

Це лише display transform. Він **не змінює** FITS, RAW, SER, OALV/WebRTC payload, дані autofocus, histogram/controller або camera acquisition.

## Межа кваліфікації

v0.2.10.59 поки є source/static checkpoint до fresh Windows MSVC build і desktop HIL. Останній підтверджений fresh Windows build — lineage v0.2.10.58-buildfix9.
