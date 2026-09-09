# Mobile Qt Quick / QML GUI — OAL 1.0 direction

OpenAstroLink 1.0 is planned to add a **mobile/touch GUI based on Qt Quick/QML**. This is a second presentation layer, not a rewrite of the node or hardware stack.

The architecture target is:

```text
OpenAstroLink Node
   ├─ Qt Widgets OpenAstroSuite — expert/engineering desktop GUI
   ├─ Qt Quick/QML client       — mobile/tablet/touch GUI (OAL 1.0)
   └─ third-party OAL clients
```

The node remains authoritative for camera/mount/focuser ownership, resource locks, long-running operations, scheduler state, safety and science acquisition. The QML client consumes the same OAL HTTP/events/WebRTC interfaces as other remote clients.

## Why not rewrite the desktop GUI now?

The Qt Widgets application already covers engineering/HIL workflows and is actively being qualified against real hardware. Rewriting it before Beta would create UI regressions without improving observatory capability. New presentation logic should nevertheless avoid embedding hardware/business logic directly inside widgets so that the future QML client can reuse the same API and controller models.

## OAL 1.0 mobile goals

- responsive phone/tablet layout;
- touch-friendly Live View and target acquisition;
- Smart Telescope UX and observing wizards;
- fullscreen/kiosk operation on Raspberry Pi displays;
- Night Vision / Strict Night presentation equivalents;
- remote Main/Guide preview through the OAL video transport;
- no local hardware drivers inside the mobile frontend.
