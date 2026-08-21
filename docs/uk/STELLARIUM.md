# Інтеграція зі Stellarium

Версія: 0.2.9

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
