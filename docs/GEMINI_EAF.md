# GeminiAstro EAF — OAL support policy

## Current status in v0.2.10.10

Gemini EAF is currently supported through **compatibility transports**, not yet through a native OAL hardware driver:

```text
Gemini EAF
  ├─ INDI  → OAL compatibility adapter   (Linux/RPi)
  └─ Alpaca → OAL compatibility adapter  (Windows/remote)
```

This path is deliberately retained so the real telescope can be used before a verified low-level Gemini protocol is available.

The project architecture is nevertheless native-first. The intended final path is:

```text
Gemini EAF → verified USB/serial protocol → oal.gemini → ABI v2 → OAL core
```

No undocumented safety-critical command is guessed merely to claim a direct driver.

## Why INDI/Alpaca remain useful

They provide immediate backward-compatible control for:

- connect/disconnect;
- absolute/relative move where exposed;
- halt;
- position/moving telemetry;
- temperature where exposed.

Current autofocus requires a reliable absolute position capability.

## Native `oal.gemini` qualification plan

1. Record exact model, firmware, USB bridge and VID/PID.
2. Obtain vendor protocol specification if possible.
3. If no specification is available, capture official vendor/ASCOM traffic for a complete command matrix.
4. Cover read status, absolute/relative movement, halt, limits, temperature, errors, reconnect and power-cycle behavior.
5. Build a golden-transcript simulator.
6. Implement the transport behind ABI v2, not directly inside `ApplicationController`.
7. Publish typed capabilities derived from the device/firmware.
8. Run native driver conformance tests.
9. Perform hardware-in-the-loop regression including mechanical-limit and disconnect scenarios.
10. Only then mark `oal.gemini` hardware-validated and prefer it over compatibility adapters.

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


## Serial-port discovery and manual selection (v0.2.10.10)

The native `oal.gemini` driver supports three discovery levels, in this order:

1. A process override (`OAL_GEMINI_PORT`, or `openastrolink-node --gemini-port COM4`).
2. A port saved from the OpenAstroSuite **Devices → Native serial discovery** panel.
3. Automatic enumeration of all serial ports returned by Qt (`QSerialPortInfo::availablePorts()`).

In the GUI, choose **Gemini EAF**, then either **Auto — scan all serial ports** or a concrete `COMx`/`/dev/tty*` port and press **Apply & rediscover native devices**. When the GUI controls a remote node, the port list comes from the node, not from the GUI computer.

The node also exposes `GET /api/v1/system/serial-ports` and `GET/POST /api/v1/drivers/serial-port`, so port selection remains available to remote clients. The selection is persisted in `QSettings`; an explicit node command-line/environment override takes precedence at startup.

Note that selecting the correct port cannot compensate for a protocol-level failure. The current native Gemini handshake sends MyFocuserPro2 `:02#` at 9600 8N1 and expects `EOK#`. If the controller produces no reply, discovery correctly leaves that port unclassified and the serial/protocol path must be diagnosed separately.

### Windows/CH340 reset-aware handshake (v0.2.10.10)

If the controller resets when the COM port is opened, `oal.gemini` performs a fast `:02#` probe and, on failure, keeps that same port open until `resetRecoveryMs` (default 2000 ms) has elapsed before retrying. This matches observed Gemini EAF HIL behavior: an immediate manual probe returned no data while a probe after a two-second post-open delay returned `EOK#`.


### Serial diagnostics in `oal-hardware-probe` (v0.2.10.10)

When Gemini discovery is pinned with `--gemini-port`, the probe now prints the driver log synchronously even though the command-line probe does not enter the Qt event loop. Diagnostics include port-open failures, first/recovery `:02#` exchanges, received text/hex bytes, and transport timeout details.

On Windows, remember that a COM port is normally exclusive. A PowerShell/.NET `SerialPort` object left open by a failed `finally` block can prevent OAL from opening the same port. Close it with `$port.Close()`, terminate the owning serial-terminal process, or unplug/replug the USB serial device before retesting.

## v0.2.10.10 — manifest timing hotfix

Windows HIL isolated a configuration-precedence bug in v0.2.10.8/v0.2.10.9. Although the C++ Gemini driver default had been increased for CH340 controller reset recovery, `oal_driver_gemini.manifest.json` still supplied `openSettleMs: 150`, and the host passes the manifest `config` object into the driver at startup. The runtime therefore continued to probe only about 150 ms after opening the port.

v0.2.10.10 synchronizes the manifest and C++ defaults to `openSettleMs: 2200` and `resetRecoveryMs: 1200`. The first `:02#` probe is sent only after the quiet-open interval; on failure the same port remains open for an additional 1200 ms before one retry. `oal-hardware-probe` also logs the effective serial timing configuration so HIL output proves which values are active.
