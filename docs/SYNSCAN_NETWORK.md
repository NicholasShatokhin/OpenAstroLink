# SynScan network compatibility

OpenAstroLink provides two network compatibility paths through a running Sky-Watcher SynScan / SynScan Pro application. These are separate from the direct native `oal.skywatcher` and `oal.eqdrive` drivers.

## Recommended: SynScan App Protocol (`synscan-app`)

The richer path uses the official **SynScan App Protocol** on UDP port 11881 by default. The protocol mirrors much of ASCOM `ITelescopeV3` and supports position, asynchronous GOTO, tracking, sync, park/unpark and pulse guiding.

The endpoint is the IP address of the **phone or PC running SynScan Pro**, not automatically the mount Wi-Fi adapter address.

Example:

```text
Backend: synscan-app
Endpoint: 192.168.4.2:11881
```

When SynScan Pro and OpenAstroLink run on the same Windows machine, `127.0.0.1:11881` is appropriate.

Reference: Sky-Watcher SynScan App Protocol (current developer documentation), <https://inter-static.skywatcher.com/downloads/synscan_app_protocol_20250930.pdf>.

## Serial-protocol-over-TCP compatibility (`synscan-wifi`)

A second, narrower backend connects to SynScan Pro's **SynScan Communication Protocol** TCP server, default port 11882, and reuses OAL's existing SynScan serial RA/Dec codec.

Example:

```text
Backend: synscan-wifi
Endpoint: 192.168.4.2:11882
```

This path is useful for compatibility/testing but the 11881 App Protocol is preferred when both are available because its telescope API is richer.

## What this is not

These two backends control a mount **through the SynScan application**. They are not direct UDP-to-mount Motor Controller transports. A direct native controller should use the corresponding native OAL driver when available.

## Typical topology

```text
OpenAstroLink node ---- Wi-Fi/LAN ---- SynScan Pro (phone/PC)
                                      |
                                      +---- mount transport ---- mount
```

The SynScan Pro screen exposes the actual configured ports; OAL endpoints can be changed accordingly.
