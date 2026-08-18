# Платформи

## Windows

Рекомендовано Qt 6.4+ MSVC kit і OpenCV того самого ABI. Serial endpoint: `COM3`. ASCOM використовується через Alpaca HTTP, тому COM interop не потрібен.

QHY: вкажіть `QHYCCD_INCLUDE_DIR` і `QHYCCD_LIBRARY` або додайте SDK у стандартні CMake search paths.

Canon/libgphoto2 зазвичай простіше залишити вимкненим на Windows або збирати через окремий package manager.

## Linux

Qt/OpenCV можуть бути системними або власною інсталяцією Qt. Для serial потрібні права на `/dev/ttyUSB*`/`/dev/ttyACM*`.

## Raspberry Pi

Збирати Release, бажано з Ninja. Для великих кадрів обмежити resolution/ROI. OAL server корисний як edge-node, а GUI може працювати на іншій машині через OAL.

## macOS

Qt/OpenCV і libgphoto2 можуть бути встановлені package manager. QHY SDK на macOS може вимагати firmware initialization відповідно до документації конкретної версії SDK.
