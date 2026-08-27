# Classic ASCOM compatibility

OpenAstroLink now has a Windows **Classic ASCOM** mount backend in addition to ASCOM Alpaca.

This is the N.I.N.A.-style compatibility path: OpenAstroLink selects a registered ASCOM Telescope driver (for example EQMOD), and that driver owns the physical COM connection to the mount/controller.

```text
OpenAstroLink node / GUI
        |
        | local JSON IPC
        v
oas-ascom-host.exe
        |
        | Windows COM / IDispatch
        v
ASCOM Telescope driver (e.g. EQMOD)
        |
        v
serial/network mount transport
```

## Why a separate helper process?

Classic ASCOM is Windows-specific and third-party drivers have their own COM threading and UI lifecycle. OAL therefore keeps the COM object out of the main node process. `oas-ascom-host.exe` is built as a small native C++ helper and uses the installed ASCOM Platform through COM automation; no .NET SDK is required to build this helper.

This is an isolation boundary, not UI automation: OpenAstroLink talks to the ASCOM driver API, not to the buttons in the driver's window.

## GUI workflow

1. Select mount backend **ascom-classic**.
2. Press **ASCOM Chooser...**.
3. Select the desired Telescope driver (for example the installed EQMOD driver).
4. Optionally press **ASCOM Properties...** to open that driver's setup dialog.
5. Press **Connect / reconnect**.

The selected ProgID is stored as the mount endpoint.

When the GUI is remote, the chooser is intentionally disabled because the ASCOM Platform and chooser UI live on the node machine. The ProgID can still be configured on the node/locally and then used remotely.

## Implemented telescope calls

The helper uses the Classic ASCOM Telescope automation surface for:

- `Connected`;
- `RightAscension`, `Declination`;
- `Tracking`, `Slewing`, `AtPark`, `SideOfPier`;
- `SlewToCoordinatesAsync` (or synchronous fallback when necessary);
- `AbortSlew`;
- `SyncToCoordinates`;
- `Park` / `Unpark`;
- `PulseGuide`.

Unsupported calls or driver COM exceptions are returned to OAL as explicit errors.

## Build/runtime

Build option:

```text
OAS_ENABLE_ASCOM_CLASSIC=ON
```

On Windows the build creates:

```text
build/<preset>/oas-ascom-host.exe
```

Runtime requires the ASCOM Platform and the selected Telescope driver to be installed/registered on the node machine. ASCOM Alpaca remains a separate backend (`ascom-alpaca`).
