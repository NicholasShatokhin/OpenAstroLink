# GeminiAstro EAF — OAL support policy

## Current status in v0.2.10

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
