# How the previous fragments were consolidated

This package is not a literal stacking of every diff produced during development. Several classes had been redefined in conflicting ways, some previously labelled “finished” backends were in fact stubs, and the console and Qt paths contained duplicated logic. Consolidation made the following deliberate changes.

## One device core

- `CaptureDevice`, multiple versions of `MountController`, and `FocuserController` were replaced by the shared `ICamera`, `IMount`, and `IFocuser` interfaces.
- The GUI, algorithms, OAL HTTP server, and OAL WebSocket layer use the same active objects through `ApplicationController`.
- There is no separate “server-side” simulation that can silently diverge from GUI state.

## Cross-platform implementation

- POSIX `termios/open/read/write` code was replaced by Qt `QSerialPort`, so the serial LX200 compatibility backend is not tied to Linux.
- Classic Windows COM ASCOM is not required; ASCOM interoperability is provided primarily through Alpaca HTTP.
- QHY, Canon EOS, ZWO ASI, and ZWO EAF are native OAL ABI-v2 plug-ins. `oal.canon` uses libgphoto2 only as its linked USB/PTP transport. Legacy `canon-gphoto2`, INDI, Alpaca, and LX200 remain optional compatibility integrations rather than the reference architecture.

## Protocol corrections

- Alpaca setter/action endpoints use HTTP `PUT` and form-encoded parameters instead of treating setters as GET commands with query strings.
- `sync` and `slew` are separate operations; a slew does not implicitly perform a sync.
- OAL uses a common `{ok,error,data}` envelope in the current REST implementation and typed WebSocket events, pending the planned RFC 9457 migration.

## Mathematical corrections

- The old “plate solver” that always returned Polaris was removed. It was replaced by a real but limited triangle/catalog prototype, an ASTAP production adapter, and a separate neural-solver interface.
- The old polar-alignment approximation that averaged `solved - mount` was removed. The current estimator derives the mechanical RA axis from relative 3D orientations of solved frames.
- The motion estimator matches stellar centroids using a robust partial affine transform.
- Autofocus uses coarse/fine scans, multiple frames per position, and distinct stellar/planetary metrics.

## What is deliberately not presented as finished

- The in-house full-sky blind solver, geometric Bahtinov spike-offset solver, durable scheduler executor, and neural runtime are not complete.
- Native QHY, Canon EOS, Gemini, Sky-Watcher, ZWO ASI, and ZWO EAF paths still require hardware-in-the-loop qualification on the exact devices/firmware before a production label.
- ABI-v2 manifest discovery and generic native camera/mount/focuser adapters are integrated, but sandboxed out-of-process hosting and the final normative capability/conformance model remain future work.
- Dual-camera ownership is implemented, but the production guide-camera calibration/dither/star-loss pipeline is not yet complete.
- The Stellarium bridge intentionally implements the standard telescope-control scope—mount position and GOTO—not the full OAL observatory API.
