# Планетарний запис SER

OpenAstroLink v0.2.10.45 може напряму записувати Live View у нестиснений SER для Місяця, планет і lucky imaging.

- Запис опційний і вмикається на **Live / Finder**.
- Кадри пишуться **до** preview debayer та auto-stretch, тому Bayer/mono samples у SER лишаються сирими.
- Preview debayer, crosshair, Mil-Dot та кутова сітка існують лише на екрані й не записуються в кадри SER.
- Writer підтримує 8/16-bit mono/Bayer і BGR потоки. Зміна геометрії або pixel format зупиняє поточний запис з явною помилкою.
- У trailer SER додаються UTC timestamps кожного кадру.
- Якщо шлях не заданий, файл створюється як `Pictures/OpenAstroLink/SER/Live_<UTC>.ser`.
- Поруч із кожним SER фіналізується `.txt` з тією самою базовою назвою. Sidecar містить requested та actual first-frame exposure/gain/offset/binning, target/measured FPS, dimensions/bit depth/SER color ID, CFA/Bayer, optical train, site, UTC start/end/duration та ознаку timestamp trailer — human-readable FireCapture-style provenance.

Для QHY/ASI планетарний запис варто робити через native streaming. FITS still-image capture лишається окремим science workflow.
