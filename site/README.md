# Site snapshot — v0.2.10.53

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
