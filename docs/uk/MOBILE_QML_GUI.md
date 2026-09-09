# Мобільний Qt Quick / QML GUI — напрям OAL 1.0

Для OpenAstroLink 1.0 заплановано **мобільний/touch GUI на Qt Quick/QML**. Це буде другий presentation layer, а не переписування Node або hardware stack.

Цільова архітектура:

```text
OpenAstroLink Node
   ├─ Qt Widgets OpenAstroSuite — expert/engineering desktop GUI
   ├─ Qt Quick/QML client       — mobile/tablet/touch GUI (OAL 1.0)
   └─ сторонні OAL clients
```

Node залишається authoritative для camera/mount/focuser ownership, resource locks, довгих operations, scheduler state, safety та science acquisition. QML-клієнт використовує ті самі OAL HTTP/events/WebRTC interfaces, що й інші remote clients.

## Чому не переписувати desktop GUI зараз

Qt Widgets GUI вже покриває engineering/HIL workflows і проходить кваліфікацію на реальному обладнанні. Rewrite перед Beta створив би багато UI regressions без приросту observatory capability. Водночас нову presentation logic варто тримати окремо від hardware/business logic, щоб майбутній QML client міг використовувати ті самі API/controller models.

## Цілі mobile GUI для OAL 1.0

- responsive layout для телефона/планшета;
- touch-friendly Live View і target acquisition;
- Smart Telescope UX та observing wizards;
- fullscreen/kiosk на Raspberry Pi display;
- еквіваленти Night Vision / Strict Night;
- remote Main/Guide preview через OAL video transport;
- жодних локальних hardware drivers усередині mobile frontend.
