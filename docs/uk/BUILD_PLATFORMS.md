# Платформи збірки — v0.2.9

Канонічний документ: `../BUILD_PLATFORMS.md`.

Основні цілі: Windows Qt 6/MSVC для GUI/розробки та 64-bit Raspberry Pi OS для observatory node. `rpi4-native-release` збирає native path без INDI, `rpi4-observatory-release` додає INDI compatibility.

Vendor SDK drivers вмикаються CMake options. QHY використовує `QHYCCD_INCLUDE_DIR/QHYCCD_LIBRARY`; ZWO ASI — `ZWO_ASI_INCLUDE_DIR/ZWO_ASI_LIBRARY`; ZWO EAF — `ZWO_EAF_INCLUDE_DIR/ZWO_EAF_LIBRARY`; Canon використовує libgphoto2 transport.

INDI легко вмикається через `OAS_ENABLE_INDI=ON` і не потрібен нативним драйверам.
