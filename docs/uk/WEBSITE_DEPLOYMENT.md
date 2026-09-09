# Deployment openastro.link

> Поточний synchronized snapshot: **v0.2.10.58-buildfix9 (2026-09-09)**.

## Поточний public state

Станом на 2026-09-09 `openastro.link` і `www.openastro.link` доступні та віддають сайт через HTTPS. Public HTML усе ще показує старіший v0.2.10.51-era snapshot, тому DNS/TLS більше не є blocker; треба опублікувати current repository tree `site/`.

## Deployment source

Повний static document root — каталог `site/`.

На hosting треба викласти **вміст** `site/`, зокрема:

- `/index.html` та `/uk/index.html`;
- `/styles.css`;
- `/about/` і `/uk/about/`;
- `/manifesto/` і `/uk/manifesto/`;
- `/docs/` і `/uk/docs/`;
- `/docs/users/`, `/docs/developers/`, `/docs/integrators/`, `/docs/drivers/` та українські mirrors;
- `/sitemap.xml` і `/robots.txt`.

Repository не прив'язаний до конкретного hosting provider. Використовуйте звичайний static-file upload/deploy provider-а і зробіть `site/` джерелом document-root content.

## Canonical host

HTML canonical URLs використовують `https://openastro.link/`. `www.openastro.link` може віддавати той самий content, але в production бажано redirect-ити один hostname на canonical, щоб уникати duplicate indexing.

## Перевірка після deploy

Перевірити:

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

Landing page має показувати `v0.2.10.58` і current buildfix9 qualification, а не старий v0.2.10.51 snapshot.

## Security boundary

Публікація static website **не** означає, що треба expose-ити `openastrolink-node` HTTP/WebSocket ports. Current pre-1.0 nodes залишаються trusted-LAN/VPN services до реалізації TLS/auth/RBAC/audit.
