# v0.2.10.58-buildfix8 — Windows Canon/ZWO EAF runtime ABI repair

Цей buildfix закриває клас помилки Windows Loader `ERROR_BAD_EXE_FORMAT` / `%1 is not a valid Win32 application` для native vendor plugins.

## Для вже зібраного дерева

Після накладання overlay **не перевстановлюйте vcpkg/WebRTC**. Спочатку виконайте:

```bat
scripts\repair_windows_vendor_runtime.cmd
```

Скрипт:

1. читає поточний `CMakeCache.txt`;
2. шукає Canon EDSDK та ZWO EAF runtimes у configured/local SDK roots;
3. читає PE COFF Machine без довіри до назв `x64`/`Release` каталогів;
4. приймає тільки `AMD64`;
5. видаляє stale однойменні DLL з build root і `drivers\`;
6. stage-ить AMD64 runtime DLL-и;
7. перевіряє архітектуру plugin/runtime DLL та, якщо доступний `dumpbin`, показує direct dependencies.

Успішний результат закінчується:

```text
WINDOWS_RUNTIME_ABI_CHECK_PASS
WINDOWS_VENDOR_RUNTIME_REPAIR_PASS
```

Після цього одразу повторіть запуск node. Повна перебудова для першого runtime retest не потрібна.

## Для наступних configure/build

CMake тепер сам перевіряє PE Machine vendor DLL. Wrong-architecture DLL не stage-иться, а якщо runtime directory містить лише несумісні DLL, configure завершується явною помилкою замість створення build, який впаде вже під час запуску.
