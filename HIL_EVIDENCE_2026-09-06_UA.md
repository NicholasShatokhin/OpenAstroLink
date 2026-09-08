# HIL evidence summary — 2026-09-06

Цей файл додається до handoff package як короткий індекс raw logs/screenshots/FITS, а не як заміна raw evidence.

## Підтверджене

- Free-point Sky Map GOTO виконувався кілька разів; у GUI log є J2000 targets, а node приймав відповідні slew operations.
- Sky Map target передався у Scheduler.
- QHY5III462C: Live View start/stop, still FITS 1920×1080 і science file paths працювали.
- Gemini focuser: moves/status працювали; AF cancellation відновлювала starting focus position.
- SER відкривається в AutoStakkert.
- Mount abort під час GOTO працював.

## Manual Wi-Fi mount control

Node log показує пари manual commands і `axis1=0 axis2=0 rate=0`, але фізично press/release іноді не спрацьовував або рух продовжувався після release. Це важливо: GUI/Core не просто «забув stop», тому hardening зроблений у UDP/11880 transport/post-condition layer, а не в geometry v9.

## Autofocus

Реальні runs показали, що scene AF міг довго сканувати shallow/noisy contrast surface. Один із runs біля 7160 мав scores приблизно 828–838 при positions 6960–7360 і обрав 6960, хоча різниця від starting point мала. Інший run біля 3500–3900 також не давав стабільного чіткого піку. Це мотивувало repeatability gate і compact near-focus search.

## Still auto-exposure

Виміряна gain-200 sequence: 6.25 → 2.8125 → 1.2656 → 0.5695 → 1.418 → 0.6381 → 1.4307 s. Raw FITS підтверджують sparse-scene behavior: 0.6381 s має P50 ≈5.42% і P99.5 ≈55.4% sensor range, тоді як 1.4307 s має P50 ≈12.38% і P99.5 = 100% (bright-tail clipping). Це і є причина переходу від global-median controller до sparse bright-tail control.

## Raw evidence files у FULL package

- `evidence/2026-09-06/gui_log.txt`
- `evidence/2026-09-06/node_log.txt`
- `evidence/2026-09-06/Screenshot_205308.png`
- `evidence/2026-09-06/Screenshot_205529.png`
- `evidence/2026-09-06/Screenshot_205924.png`

FITS-файли не дублюються у compact handoff ZIP, щоб не роздувати пакет; їхні filenames і ключові measured metrics зафіксовані тут.
