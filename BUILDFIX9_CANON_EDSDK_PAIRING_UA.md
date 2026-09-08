# OpenAstroLink v0.2.10.58-buildfix9 — Canon EDSDK pairing

Під час AMD64 configure buildfix8 правильно зупинив staging x86 runtime з `C:/SDK/EDSDK/Windows/EDSDK/Dll`, хоча import library була x64: `C:/SDK/EDSDK/Windows/EDSDK_64/Library/EDSDK.lib`.

buildfix9 робить import library і runtime єдиною парою. Для вибраної `EDSDK.lib` CMake бере батьківський SDK-архітектурний каталог і шукає sibling `Dll/EDSDK.dll`, перевіряє його PE Machine та примусово оновлює `CANON_EDSDK_RUNTIME_DIR`. Для x64 import library очікуваний runtime у цьому layout: `C:/SDK/EDSDK/Windows/EDSDK_64/Dll`.

Після накладання overlay достатньо повторити `cmake --preset my-windows-observatory`, а потім build. Повторний bootstrap libdatachannel не потрібен.
