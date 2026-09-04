## v0.2.10.51 build/distribution status

Native vendor SDK discovery/bootstrap є частиною supported build workflow. QHY 26.06.04 ARM64 перевірено повною Raspberry Pi cross-збіркою; QHY/ZWO staging перевіряє target architecture, а не довіряє назві archive. Canon EDSDK — тільки manual-download/local-discovery. Native drivers — default; INDI — optional compatibility.

# Native OAL Driver SDK — ABI v2

Канонічний документ: `../NATIVE_DRIVER_SDK.md`.

ABI v2 використовує стабільний C boundary, manifest, identity/capabilities, invoke/cancel, push events, health і native frame publication. Driver має оголошувати thread model, transport/permissions та не переносити великі science frames через JSON/Base64. Native driver може використовувати vendor SDK як low-level hardware transport; це не робить його compatibility backend.

У v0.2.10.5 reference drivers: simulated, QHY, Canon, ZWO ASI/EAF, Gemini та Sky-Watcher. Out-of-process sandbox залишається важливим наступним hardening layer.
