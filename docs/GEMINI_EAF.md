# GeminiAstro EAF — OAL support policy

## Current status in v0.2.10.11

Gemini EAF now has a **native OAL ABI-v2 driver path** and has passed basic Windows hardware-in-the-loop validation on real USB-serial hardware:

```text
Gemini EAF → USB/serial (9600 8N1) → oal.gemini → ABI v2 → OAL core
```

Confirmed on hardware:

- serial discovery and connection;
- MyFocuserPro2-compatible `:02# → EOK#` identification;
- status/temperature traffic;
- direct focuser motion;
- focuser motion driven by the node's autofocus operation;
- cancellation of autofocus by the user.

INDI and Alpaca remain compatibility fallbacks, but native `oal.gemini` is now the preferred path when the device is discovered successfully. Remaining HIL work includes reconnect/power-cycle behavior, mechanical limits, HALT under stress, long-run stability and autofocus convergence/repeatability on a real optical target.

## Why INDI/Alpaca remain useful

They provide immediate backward-compatible control for:

- connect/disconnect;
- absolute/relative move where exposed;
- halt;
- position/moving telemetry;
- temperature where exposed.

Current autofocus requires a reliable absolute position capability.

## Native `oal.gemini` qualification plan

Completed foundation/HIL items: native ABI-v2 transport, serial discovery, real-device handshake, connection, status/temperature traffic, direct motion and autofocus-driven motion.

Remaining qualification work:

1. Record/stabilize an exact firmware/USB-bridge compatibility matrix.
2. Validate absolute position across reconnect and power cycle.
3. Validate relative/absolute movement near configured mechanical limits.
4. Validate HALT during a longer move and during cancellation races.
5. Validate disconnect/reconnect and USB removal during motion.
6. Build/extend a golden-transcript simulator from the confirmed protocol traffic.
7. Expand typed capabilities and error mapping from observed firmware behavior.
8. Run repeated native-driver conformance/HIL cycles.
9. Validate autofocus convergence and repeatability on real stars/planetary targets.
10. Only after those tests mark the driver ready for unattended operation.

## Compatibility HIL checklist now

Before using the compatibility path for unattended autofocus:

1. Connect/disconnect repeatedly.
2. Confirm position after power cycle.
3. Small `+100` and `-100` moves.
4. Safe absolute move.
5. HALT during a longer move.
6. Verify moving state if available.
7. Verify temperature if a probe exists.
8. Verify lower/upper limits and no wrap/overflow.
9. Run repeated autofocus scans from both sides of focus.
10. Disconnect USB/network during movement and verify OAL reports error/unknown rather than false success.
11. Record exact firmware/driver/OS versions in the validation log.

## Architectural rule

The Gemini compatibility profile must never become the specification for a future native focuser driver. Native OAL may expose richer precision, limits, telemetry, events, cancellation and calibration semantics even if INDI/Alpaca cannot represent all of them.


## Serial-port discovery and manual selection (v0.2.10.11)

The native `oal.gemini` driver supports three discovery levels, in this order:

1. A process override (`OAL_GEMINI_PORT`, or `openastrolink-node --gemini-port COM4`).
2. A port saved from the OpenAstroSuite **Devices → Native serial discovery** panel.
3. Automatic enumeration of all serial ports returned by Qt (`QSerialPortInfo::availablePorts()`).

In the GUI, choose **Gemini EAF**, then either **Auto — scan all serial ports** or a concrete `COMx`/`/dev/tty*` port and press **Apply & rediscover native devices**. When the GUI controls a remote node, the port list comes from the node, not from the GUI computer.

The node also exposes `GET /api/v1/system/serial-ports` and `GET/POST /api/v1/drivers/serial-port`, so port selection remains available to remote clients. The selection is persisted in `QSettings`; an explicit node command-line/environment override takes precedence at startup.

Note that selecting the correct port cannot compensate for a protocol-level failure. The current native Gemini handshake sends MyFocuserPro2 `:02#` at 9600 8N1 and expects `EOK#`. If the controller produces no reply, discovery correctly leaves that port unclassified and the serial/protocol path must be diagnosed separately.

### Windows/CH340 reset-aware handshake (v0.2.10.11)

If the controller resets when the COM port is opened, `oal.gemini` keeps the port quiet for `openSettleMs` (currently 2200 ms) before the first `:02#` probe. On failure it keeps that same port open for the additional `resetRecoveryMs` interval (currently 1200 ms) before retrying. This matches observed Gemini EAF HIL behavior: an immediate manual probe returned no data while a probe after a two-second post-open delay returned `EOK#`.


### Serial diagnostics in `oal-hardware-probe` (v0.2.10.11)

When Gemini discovery is pinned with `--gemini-port`, the probe now prints the driver log synchronously even though the command-line probe does not enter the Qt event loop. Diagnostics include port-open failures, first/recovery `:02#` exchanges, received text/hex bytes, and transport timeout details.

On Windows, remember that a COM port is normally exclusive. A PowerShell/.NET `SerialPort` object left open by a failed `finally` block can prevent OAL from opening the same port. Close it with `$port.Close()`, terminate the owning serial-terminal process, or unplug/replug the USB serial device before retesting.

## v0.2.10.11 — manifest timing hotfix

Windows HIL isolated a configuration-precedence bug in v0.2.10.8/v0.2.10.9. Although the C++ Gemini driver default had been increased for CH340 controller reset recovery, `oal_driver_gemini.manifest.json` still supplied `openSettleMs: 150`, and the host passes the manifest `config` object into the driver at startup. The runtime therefore continued to probe only about 150 ms after opening the port.

v0.2.10.11 synchronizes the manifest and C++ defaults to `openSettleMs: 2200` and `resetRecoveryMs: 1200`. The first `:02#` probe is sent only after the quiet-open interval; on failure the same port remains open for an additional 1200 ms before one retry. `oal-hardware-probe` also logs the effective serial timing configuration so HIL output proves which values are active.
