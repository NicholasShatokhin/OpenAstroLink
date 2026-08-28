# SynScan network transports

OpenAstroLink deliberately exposes two different network backends because the
Sky-Watcher Wi-Fi adapter and the SynScan App/Pro application are different
protocol endpoints.

## Direct mount Wi-Fi — `synscan-wifi`

`synscan-wifi` connects **directly to the mount / EQDrive / SynScan Wi-Fi
adapter**. SynScan Pro is not required.

Transport:

```text
OpenAstroLink -> UDP 11880 -> mount/EQDrive Wi-Fi adapter -> motor controller
```

The payload is the official Sky-Watcher **Motor Controller Command Set**. The
same low-level command family is used by EQMOD-compatible controllers.

Examples:

```text
Backend:  synscan-wifi
Endpoint: auto
```

or:

```text
Backend:  synscan-wifi
Endpoint: 192.168.4.1:11880
```

`auto` sends only a read-only motor-controller firmware query on UDP 11880 and
accepts a syntactically valid controller response. The two common AP gateway
addresses `192.168.4.1` and `192.168.0.1` are also tried because some adapters
do not answer broadcast discovery.

Direct RA/DEC coordinates use a conservative sync-anchor model. Sync once on a
known sky position before direct GOTO. Until axis direction/pier handling is HIL
qualified, one direct native GOTO is limited to a small angular delta.

Reference: Sky-Watcher Motor Controller Command Set,
<https://inter-static.skywatcher.com/downloads/skywatcher_motor_controller_command_set.pdf>.

## Through SynScan Pro — `synscan-app`

`synscan-app` is the higher-level compatibility path through a **running
SynScan App/Pro** on a phone or PC.

Transport:

```text
OpenAstroLink -> UDP 11881 -> SynScan Pro (phone/PC) -> mount
```

The endpoint is the IP of the **device running SynScan Pro**, not the mount
Wi-Fi adapter.

```text
Backend:  synscan-app
Endpoint: auto
```

or:

```text
Backend:  synscan-app
Endpoint: 192.168.0.100:11881
```

`auto` broadcasts the harmless `ServerVersion` request on UDP 11881 and uses
the responding SynScan App/Pro host. This backend exposes the richer SynScan
App telescope API: position, asynchronous GOTO, tracking, sync, park/unpark and
pulse guide when the app/mount supports them.

Reference: Sky-Watcher SynScan App Protocol,
<https://inter-static.skywatcher.com/downloads/synscan_app_protocol_20250930.pdf>.

## Port 11882

TCP 11882 is the **SynScan Communication Protocol server exported by SynScan
App/Pro**. It is not the direct mount-Wi-Fi endpoint. OpenAstroLink v0.2.10.18
no longer labels this path `synscan-wifi`; direct Wi-Fi uses UDP 11880 instead.
