# Site snapshot — v0.2.10.58-buildfix9

Windows x64/MSVC buildfix9 fresh-build qualified з увімкненим WebRTC. Активні libdatachannel runtime staging, ZWO EAF DLL-import linkage, vendor PE-machine validation і Canon `EDSDK_64` import/runtime pair locking. Найближчий runtime gate — load `oal.canon` і `oal.zwo.eaf` із corrected build, далі QHY/WebRTC/Dual-Live HIL.

Містить новий розділ offline Sky Map navigation.

## v0.2.10.50 snapshot сайту

Landing page оновлено під підтверджені Windows/Linux/Raspberry-Pi ARM64 builds, native-first/INDI-opt-in policy, HIL-qualified mount v9 без тимчасового driver GOTO cap та поточні Beta-пріоритети. Pi 5 позначено як ARM64 ABI-supported з physical qualification pending; macOS — configured/pending physical build.

# Вихідні файли сайту openastro.link

Це початкова статична версія сайту `https://openastro.link/`.

- `index.html` — канонічна англійська сторінка.
- `uk/index.html` — українське дзеркало.
- `styles.css` — спільне адаптивне оформлення.

Провайдер розгортання поки не фіксується. Каталог можна публікувати через GitHub Pages, Cloudflare Pages, Netlify, nginx або інший статичний хостинг. DNS/TLS варто налаштовувати після вибору deployment target.


Sky Map v0.2.10.52 додає наведення у довільну видиму точку неба через той самий OAL mount/Scheduler path.

Sky Map v0.2.10.53 додає measured solved camera rectangles, predicted main/guide footprints, Scheduler mosaic preview та optional Stellarium Remote Control frame export.

WebRTC checkpoint: `/webrtc` signaling + `oalv-main`/`oalv-guide` OALW v1 DataChannel; `/video` fallback збережено.

## Public information architecture

Сайт тепер публікує rationale проєкту, manifesto та окремі documentation routes для астрономів/users, OAL core developers, third-party integrators і driver authors. Static paths: `/uk/about/`, `/uk/manifesto/`, `/uk/docs/`, `/uk/docs/users/`, `/uk/docs/developers/`, `/uk/docs/integrators/`.
