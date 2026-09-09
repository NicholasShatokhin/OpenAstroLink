# Site snapshot — v0.2.10.59

Windows x64/MSVC buildfix9 is fresh-build qualified with WebRTC enabled. libdatachannel runtime staging, ZWO EAF DLL-import linkage, vendor PE-machine validation, and Canon `EDSDK_64` import/runtime pair locking are active. The next immediate runtime gate is loading `oal.canon` and `oal.zwo.eaf` from the corrected build, followed by QHY/WebRTC/Dual-Live HIL.

Includes the new offline Sky Map navigation section.

## v0.2.10.50 site snapshot

The landing page now reflects confirmed Windows/Linux/Raspberry-Pi ARM64 build status, native-first/INDI-opt-in policy, HIL-qualified mount v9 with the temporary driver GOTO cap removed, and the current Beta workflow priorities. Pi 5 is described as ARM64 ABI-supported with physical qualification pending; macOS remains configured/pending physical build.

# openastro.link site source

This directory is the initial static site for `https://openastro.link/`.

- `index.html` — canonical English landing page.
- `uk/index.html` — Ukrainian mirror.
- `styles.css` — shared responsive styling.

No deployment provider is assumed yet. The directory can be published as static files by GitHub Pages, Cloudflare Pages, Netlify, nginx, or another host. DNS/TLS should be configured only after choosing the deployment target.


Sky Map v0.2.10.52 adds arbitrary visible-sky point targeting with the same OAL mount/Scheduler path.

Sky Map v0.2.10.53 adds measured solved camera rectangles, predicted main/guide footprints, Scheduler mosaic preview and optional Stellarium Remote Control frame export.

WebRTC checkpoint: `/webrtc` signaling + `oalv-main`/`oalv-guide` OALW v1 DataChannels; `/video` fallback retained.

## Public information architecture

The site now publishes the project rationale, manifesto, and separate documentation routes for astronomers/users, OAL core developers, third-party integrators and driver authors. Static paths: `/about/`, `/manifesto/`, `/docs/`, `/docs/users/`, `/docs/developers/`, `/docs/integrators/` plus Ukrainian `/uk/...` mirrors.
