# Інтеграція зі Stellarium

Версія: 0.2.10

OpenAstroLink має прямий TCP-міст, сумісний із зовнішнім Stellarium Telescope Control protocol. Це міст для монтування, а не заміна повного API обсерваторії OAL.

## Що працює

- OAL передає поточні RA/Dec монтування у Stellarium.
- GOTO зі Stellarium декодується і передається активному OAL mount.
- Міст працює на Raspberry Pi node незалежно від того, GUI локальний чи віддалений.
- Його можна налаштовувати через GUI або REST і зберігати в settings.

Типовий порт: `10000`.

```bash
openastrolink-node --stellarium-port 10000
```

REST: `GET/POST /api/v1/integrations/stellarium`.

## Межа можливостей

Стандартне telescope-control з'єднання Stellarium передає положення телескопа і GOTO, але не повну OAL-модель камер, фокусерів, autofocus, polar alignment, safety, operations та sessions. Для них лишається OAL GUI/REST/WebSocket. Надалі можна створити окремий Stellarium OAL plug-in з повною панеллю обсерваторії.

## Безпека

Міст використовує те саме активне монтування і підпорядковується locks та hardware limits. Не відкривайте TCP-порт напряму в Інтернет; використовуйте LAN/VPN.

## Live position

The bridge publishes the active mount position to Stellarium every 500 ms and sends an immediate position update when a Stellarium client connects. The packet is normalized to J2000. Raw-axis EQDrive needs one OAL Sync before a valid encoder-to-sky position exists.

## Повторюваний Home workflow (v0.2.10.38)

Для native raw-axis екваторіального монтування, яке перед увімкненням фізично повертається в одну й ту саму Home-позицію (вісь вантажів вниз, труба/DEC-вісь у бік небесного полюса), один раз виконайте **Set current mechanical axes as Home** та увімкніть **Assume saved Home on connect**. При наступних підключеннях OpenAstroLink порівнює raw axes зі збереженим Home і, якщо вони в межах tolerance, автоматично відновлює encoder-to-sky модель. Plate-solve Sync можна використовувати для уточнення наведення, але він більше не потрібен лише для того, щоб Stellarium GOTO почав працювати.

Зміна sky-separation GOTO safety limit, Park coordinates або Home preference більше не скидає Sync. Зміна знака осі, geometry, pier branch, latitude або longitude все ще змінює transform і може потребувати automatic Home restore (якщо монтування зараз у Home) або нового Sync.

## v0.2.10.38: near-pole safety та ASCOM site

Native safety limit тепер перевіряє реальну angular separation на небі, а не raw RA-axis rotation; тому невеликий рух біля полюса більше не відхиляється лише через сингулярність RA. Raw transport окремо обмежений 180° на вісь.

Для Classic ASCOM OAL перевіряє site самого driver перед GOTO. Якщо EQMOD має інші latitude/longitude, команда не повинна виконуватись, доки site не буде виправлено. У EQMOD Setup може знадобитися **Allow Site Writes**. Axis1/Axis2 inversion OAL не застосовується до ASCOM.
