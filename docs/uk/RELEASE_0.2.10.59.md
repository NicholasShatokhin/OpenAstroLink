# OpenAstroLink v0.2.10.59 — Night Vision source checkpoint

Цей checkpoint додає desktop **Night Vision**, **Strict Night Mode** та опціональну чорно-червону display palette для монохромного/RAW Live View без debayer. Також зафіксовано план окремого mobile Qt Quick/QML frontend для OAL 1.0.

## Desktop themes

- `View → Night vision mode → Normal / Night Vision / Strict Night Mode`.
- `Ctrl+Shift+N` циклічно перемикає три режими.
- Вибір зберігається і застосовується ще до core-selection dialog.
- Strict mode переводить custom graphics/overlays у red-only; звичайний Night Vision може зберігати семантичні кольори custom-графіки.

## Camera preview

- Night/Strict самі по собі не перефарбовують camera pixels.
- `Black→red palette for monochrome/raw preview` працює тільки для справді монохромного display і для Main лише коли debayer вимкнений.
- Science FITS/RAW/SER, acquisition та OAL video transport не змінюються.

## Майбутній GUI

Для OAL 1.0 запланований окремий mobile/touch Qt Quick/QML client поверх тієї ж Node/API/WebRTC архітектури. Поточний Qt Widgets OpenAstroSuite залишається expert/engineering desktop client.

## Кваліфікація

Поки лише source/static qualification до fresh Windows build і UI HIL. v0.2.10.58-buildfix9 залишається останнім fresh Windows build-qualified checkpoint.
