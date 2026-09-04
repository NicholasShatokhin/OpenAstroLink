# Classic ASCOM compatibility


> **Current release:** v0.2.10.51. Classic ASCOM remains an optional Windows compatibility backend; native OAL drivers are the default.

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
- `Tracking`, `Slewing`, `AtPark`, `AtHome`, `SideOfPier`;
- `SlewToCoordinatesAsync` (or synchronous fallback when necessary);
- `AbortSlew`;
- `SyncToCoordinates`;
- `Park` / `Unpark` and persistent `SetPark` when `CanSetPark` is true;
- `PulseGuide`;
- `SiteLatitude`, `SiteLongitude`, `SiteElevation`, `UTCDate`;
- driver-reported `Azimuth`, `Altitude`, `SiderealTime`, `EquatorialSystem` and capabilities for diagnostics.

Unsupported calls or driver COM exceptions are returned to OAL as explicit errors.

## Site, epoch and EQMOD compatibility

OpenAstroLink keeps its observatory profile as the canonical location. On Classic ASCOM connect and immediately before a GOTO, OAL compares the driver-reported site with that profile. A wrong site can produce a physically mirrored East/West slew even when the RA/DEC command itself is correct. If the ASCOM driver permits site writes, OAL automatically writes the profile site/time and verifies the result.

EQMOD commonly ships with **Allow Site Writes** disabled. If the site differs and the write is rejected, OAL deliberately blocks GOTO and reports an actionable error. Open **ASCOM Properties...**, set the same latitude/longitude as OAL and/or enable **ASCOM Options → Allow Site Writes**, then reconnect.

`EquatorialSystem` is interpreted according to ASCOM: Topocentric=1 and J2000=2. Older/default EQMOD configurations can advertise `equOther=0`. For `EQMOD.*` only, OAL uses an explicit compatibility assumption that this means the default EQMOD topocentric/JNow-style coordinate stream and records that assumption in diagnostics. For reproducibility, explicitly select JNOW or J2000 in EQMOD setup rather than leaving the epoch Unknown.

Axis-sign inversion in the OAL Mount tab applies only to native raw-axis drivers. It never changes Classic ASCOM behavior because EQMOD/ASCOM owns its own motor-to-sky transform.

## Persistent shared Park

The Mount tab action **Calibrate current physical pose as persistent Home / Park** has backend-specific storage but a common physical meaning. For native EQDrive it stores the current raw axes as OAL Home and Park and enables automatic Home alignment. For Classic ASCOM it calls standard `SetPark` when the driver reports `CanSetPark`.

To make native and EQMOD/ASCOM park to the same physical pose, put the mount in that pose once, calibrate the native backend, switch backend without moving the mount, and calibrate Classic ASCOM once. Both calibrations persist; this is not a per-session operation.

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
