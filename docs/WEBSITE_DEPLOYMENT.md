# openastro.link deployment

> Current synchronized snapshot: **v0.2.10.58-buildfix9 (2026-09-09)**.

## Current public state

As observed on 2026-09-09, both `openastro.link` and `www.openastro.link` are reachable and serve the site over HTTPS. The public HTML still identifies an older v0.2.10.51-era snapshot, so DNS/TLS are no longer the deployment blocker; publishing the current repository `site/` tree is.

## Deployment source

The complete static document root is `site/`.

Publish the **contents** of `site/` at the web document root, including:

- `/index.html` and `/uk/index.html`;
- `/styles.css`;
- `/about/` and `/uk/about/`;
- `/manifesto/` and `/uk/manifesto/`;
- `/docs/` and `/uk/docs/`;
- `/docs/users/`, `/docs/developers/`, `/docs/integrators/`, `/docs/drivers/` and Ukrainian mirrors;
- `/sitemap.xml` and `/robots.txt`.

The repository does not assume a specific hosting provider. Use the provider's normal static-file upload/deploy mechanism and make `site/` the source/document-root content.

## Canonical host policy

The HTML canonical URLs use `https://openastro.link/`. It is acceptable for `www.openastro.link` to serve the same content, but production hosting should ideally redirect one hostname to the canonical one to avoid duplicate indexing.

## Post-deploy verification

Verify both names and the new content:

```text
https://openastro.link/
https://www.openastro.link/
https://openastro.link/about/
https://openastro.link/manifesto/
https://openastro.link/docs/
https://openastro.link/docs/users/
https://openastro.link/docs/developers/
https://openastro.link/docs/integrators/
https://openastro.link/docs/drivers/
https://openastro.link/uk/
```

The landing page should show `v0.2.10.58` and the current buildfix9 qualification, not the older v0.2.10.51 snapshot.

## Security boundary

Publishing the static website does **not** imply exposing `openastrolink-node` HTTP/WebSocket ports. Current pre-1.0 nodes remain trusted-LAN/VPN services until TLS/auth/RBAC/audit are implemented.
