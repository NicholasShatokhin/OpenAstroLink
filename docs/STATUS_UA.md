
### v0.2.10.49 build-fix14 — офіційний QHY 26.x ARM64

ARM64 build більше не залежить від `QHYCCD_Linux_New` для QHY. Доданий окремий downloader офіційного SDK з підтримкою нової QHY packaging scheme від 26.06.04 (`sdk_linux_arm64_<version>.tar.gz`) і staging лише бібліотек з реальною AArch64 ELF-архітектурою. Підтримуються shared і static (`libqhyccd.a`) форми SDK; shared має пріоритет. Для першої фізичної перевірки pin — QHY SDK 26.06.04. Legacy QHY ARMHF staging лишається окремо. Mount v9 не змінювався.

### v0.2.10.49 build-fix15 — closure BLAS/LAPACK alternatives

Фізичний ARM64 build уже підтвердив офіційний QHYCCD 26.06.04 path до рівня лінкування драйвера: завантажений SDK містить справжній AArch64 `libqhy.so`, а `oal_driver_qhy.so` успішно збирається поруч із Canon EDSDK і ZWO. Фінальний link executable все ще впав, бо runtime SONAME BLAS/LAPACK у Bookworm знаходяться в alternatives-managed каталогах `.../<multiarch>/blas` і `.../<multiarch>/lapack`. Попередній repair symlink у sysroot також запускався без root privileges, що видно з великого блоку `Permission denied`. build-fix15 виправляє links як root, додає ці numerical subdirectories у link-time search, перевіряє target ELF architecture і явно лінкує target LAPACK/BLAS після OpenCV. Fix підготовлений і очікує наступного фізичного WSL -> AArch64 link run. Mount v9 не змінювався.
