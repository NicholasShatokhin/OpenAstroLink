# OpenAstroLink Manifesto

OpenAstroLink exists to make astronomical equipment **open to control, open to automate, and open to integrate** without making a single GUI, operating system, vendor SDK or compatibility daemon the permanent center of an observatory.

## Principles

### 1. Hardware belongs to the node

The process closest to the equipment owns the device sessions. User interfaces, scripts and remote applications are clients. Closing or replacing a GUI must not redefine who owns the mount, camera or focuser.

### 2. Native first, compatibility always welcome

Direct vendor SDKs and hardware protocols are first-class OAL backends when they provide useful capabilities or latency. INDI, ASCOM/Alpaca, LX200 and other ecosystems remain interoperability paths. Native-first is an architectural choice, not a rejection of existing communities.

### 3. Capabilities before assumptions

Software should ask a device what it can do. A camera, mount, focuser or future observatory device should publish identity and capabilities so clients do not depend on vendor-name conditionals.

### 4. Astronomy actions are operations

A real observing action has a lifecycle. Operations can queue, run, report progress, own resources, fail and be cancelled. This model should be shared by interactive control and automation.

### 5. Recording is more important than preview

Scientific data paths must not be hostage to GUI rendering or network congestion. Preview may adapt or drop frames. Authoritative FITS/RAW/SER acquisition must preserve its own timing and provenance.

### 6. Remote is not an afterthought

The same node should serve a local GUI, a remote workstation, a script, a mobile client or another astronomy application through documented protocol surfaces.

### 7. Safety is a separate layer, not hidden geometry

Mechanical guards, sky-safety policy and operator limits must be explicit. Driver geometry must not secretly change to imitate a safety rule. HIL-qualified geometry is frozen until new evidence justifies a change.

### 8. Failure behaviour is part of the interface

Cancel, reconnect, dropped preview, device disappearance, partial network failure and restart recovery are not edge trivia. They are observatory behaviour and must be testable.

### 9. Evidence over confidence

A feature is not called hardware-qualified because it looks correct in code. Build qualification, simulation/regression evidence and real hardware-in-the-loop evidence are recorded separately.

### 10. OpenAstroSuite is a reference client, not a gatekeeper

Third-party applications must be able to use OAL without embedding or imitating the OpenAstroSuite GUI. The protocol, driver ABI and integration documentation are public project surfaces.

### 11. Users own their data and workflow

Science frames, capture metadata, calibration provenance and observing plans should remain understandable and exportable. The system should favour open formats and reproducible metadata.

### 12. Grow toward unattended operation deliberately

Supervised Beta and unattended 1.0 are different safety claims. Authentication, audit, weather/roof/power policy, durable recovery and emergency behaviour must be completed before unattended use is presented as production-ready.

## Promise to contributors and users

OpenAstroLink will document qualification boundaries, preserve HIL-proven invariants, keep English canonical documentation with Ukrainian mirrors, and prefer explicit interoperable interfaces over hidden application coupling.
