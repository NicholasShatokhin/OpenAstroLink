# Як було зведено попередні фрагменти

Цей пакет не є буквальним накладанням усіх дифів. У чаті кілька класів перевизначалися суперечливими способами, частина «готових» бекендів фактично була заглушками, а консольний і Qt-код мали окремі копії логіки. Під час зведення зроблено такі зміни.

## Єдине ядро пристроїв

- `CaptureDevice`, кілька версій `MountController` і `FocuserController` замінені інтерфейсами `ICamera`, `IMount`, `IFocuser`.
- GUI, алгоритми, OAL HTTP server і OAL WebSocket використовують ті самі активні об’єкти через `ApplicationController`.
- Немає окремої «серверної» симуляції, яка мовчки розходиться зі станом GUI.

## Кросплатформеність

- POSIX `termios/open/read/write` замінено на Qt `QSerialPort`, тому serial LX200 backend не прив’язаний до Linux.
- Класичний Windows COM-ASCOM не є обов’язковим: ASCOM підтримується через Alpaca HTTP.
- QHY винесено з core у native OAL ABI-v2 plug-in; Canon/libgphoto2 та INDI залишаються опційними compatibility integrations. Базова збірка не вимагає vendor SDK.

## Виправлені протокольні помилки

- Alpaca setter/action endpoints використовують HTTP `PUT` і form-urlencoded parameters, а не GET-команди з query string.
- `sync` і `slew` розділені: slew не викликає sync.
- OAL має однаковий envelope `{ok,error,data}` для REST-відповідей та типізовані WebSocket-події.

## Виправлена математика

- Старий «plate solver», який завжди повертав Полярну, вилучено. Замість нього є реальний, але обмежений triangle/catalog prototype і окремий neural interface.
- Старий City PA, що усереднював `solved - mount`, вилучено. Нова реалізація оцінює механічну вісь RA з відносних 3D-орієнтацій solved кадрів.
- Motion estimator зіставляє зоряні центроїди через robust partial affine transform.
- Autofocus має coarse/fine scan, кілька кадрів на позицію і окремі метрики для зірок та планети.

## Що навмисно не маскується під готовий продукт

- Blind full-sky solver, точний Bahtinov spike-offset solver, повний scheduler executor і neural runtime ще не завершені.
- Native QHY, Canon та compatibility INDI потребують перевірки на конкретному обладнанні/версіях.
- ABI-v2 manifest registry і generic native camera/mount/focuser adapters уже інтегровані в device registry; sandboxed out-of-process host та повна нормативна capability/conformance model ще попереду.
