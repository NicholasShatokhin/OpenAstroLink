# Why OpenAstroLink exists

> Canonical public rationale for OpenAstroLink / OpenAstroSuite. Current development checkpoint: **v0.2.10.58-buildfix9**.

## What OpenAstroLink is

OpenAstroLink (OAL) is an open control layer and protocol for astronomical equipment and observing workflows. `openastrolink-node` owns hardware sessions and long-running operations; OpenAstroSuite and third-party clients use the OAL HTTP/event/streaming surfaces instead of reaching directly into vendor SDKs.

The project currently covers native cameras, mounts and focusers; plate solving; autofocus; high-rate planetary capture; scheduling; mosaic execution; Polar Alignment foundations; Stellarium integration; and compatibility paths such as ASCOM/Alpaca/LX200/INDI where useful.

## Why it was created

The project began from a practical requirement: one observatory stack should be able to run locally on a Windows machine or Raspberry Pi, be controlled remotely without moving hardware ownership into the GUI, and execute an observing operation even if the user interface is not continuously responsible for every device call.

That led to several design requirements that are difficult to bolt onto a GUI-centric application after the fact:

1. **The node owns the hardware.** A GUI is a client, not the lifetime owner of a camera, mount or focuser session.
2. **Long-running astronomy tasks are operations.** Slew, exposure, autofocus, solve, scheduler blocks and future safety workflows need state, progress, cancellation and resource ownership.
3. **Scientific acquisition is separate from preview.** A dropped GUI preview frame must not imply a dropped SER/science frame.
4. **Capabilities are discovered rather than assumed.** Different devices expose different controls and constraints.
5. **Native hardware access is first-class.** Vendor SDKs and direct hardware protocols can be used without requiring a separate compatibility server in the normal path.
6. **Compatibility remains valuable.** INDI, ASCOM/Alpaca and other established interfaces are integration options, not enemies of the project.
7. **Remote control is part of the architecture.** HTTP, event streaming and preview transport are protocol surfaces rather than GUI implementation details.
8. **HIL evidence matters.** Hardware geometry, timing and failure behaviour are qualified on real equipment and frozen once proven.

## Why existing solutions were not enough for this project

OpenAstroLink is not based on the claim that INDI, ASCOM, Alpaca, N.I.N.A., KStars/Ekos or other astronomy projects are bad. They solve real problems and remain useful interoperability targets. The gap was architectural: the OpenAstroLink project needed a combination of properties that was not available as one coherent, open, native-first stack for its target workflows.

### GUI ownership versus node ownership

Many astronomy applications are primarily desktop applications. That is convenient for interactive use, but it makes remote operation, headless execution and workflow recovery depend strongly on the application process. OAL instead treats the node as the equipment owner and clients as replaceable views/controllers.

### Device APIs versus workflow semantics

Traditional device interfaces are excellent at saying “move focuser”, “slew mount” or “start exposure”. The project also needed a common model for an operation that reserves resources, reports progress, can be cancelled, and can be composed into scheduler, autofocus, solve/recenter, mosaic and Polar Alignment workflows.

### Preview traffic versus scientific data

Planetary imaging can run at tens or hundreds of frames per second. Sending every frame through a UI-oriented JSON path or coupling preview rendering to acquisition is unacceptable when the recording must remain authoritative. OAL therefore separates the camera acquisition/recording path from droppable preview transports such as OALV/WebRTC.

### Native-first hardware without abandoning compatibility

The project needed direct QHY, ZWO, Canon, Gemini and Sky-Watcher/EQDrive paths for low-latency control and hardware-specific features, while still retaining compatibility adapters. OAL makes native drivers the default architecture and keeps compatibility optional.

### A protocol intended for other software

OpenAstroLink is meant to be usable without OpenAstroSuite. A planetarium, automation service, mobile client, observatory dashboard or future application should be able to discover devices, inspect capabilities, start operations and receive state/events through the same control layer.

## What OpenAstroLink is not

- It is **not** a claim that every existing astronomy ecosystem should be replaced.
- It is **not** yet an unattended-observatory safety platform; current Beta work remains supervised.
- It is **not** a frozen 1.0 public protocol yet; some payloads and reliability semantics remain under qualification.
- It is **not** a GUI-only project. OpenAstroSuite is the reference client; OAL is the control/protocol layer.

## The intended outcome

The long-term goal is an open observatory substrate where hardware vendors can provide drivers, application developers can build independent clients, and astronomers can combine equipment without binding their observing workflow to one desktop process or one vendor SDK.

See also: `MANIFESTO.md`, `ARCHITECTURE.md`, `OAL_SPECIFICATION.md`, `OAL_API.md`, `NATIVE_DRIVER_SDK.md`, and `CURRENT_CHECKLIST.md`.
