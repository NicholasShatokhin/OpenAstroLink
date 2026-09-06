# OpenAstroLink / OpenAstroSuite v0.2.10.52

## Sky Map free-point targeting

v0.2.10.52 extends the offline Sky Map introduced in v0.2.10.51 so telescope navigation is no longer limited to catalogue objects.

- A left click on empty space inside the visible horizon circle creates a free-point target.
- The clicked screen point is inverted into horizontal Az/Alt and converted once to a J2000 coordinate through the existing `horizontalToEquatorial` path.
- The free point is rendered with a yellow `Target` marker and exposes the same RA/DEC and Alt/Az readout as catalogue targets.
- **Slew**, **Sync**, double-click GOTO and **Use in Scheduler** operate on free-point targets through the same `ObservatoryController` / active OAL mount backend path.
- Catalogue objects retain click priority when the cursor is close to an object marker.
- Clicks outside the horizon circle are ignored.
- Free-point Scheduler targets receive a coordinate-bearing name such as `Sky point RA 18.247h Dec +32.14°`.

The map converts the clicked direction to J2000 at selection time. It does not store a permanently fixed Alt/Az direction, so normal telescope tracking/scheduling semantics remain equatorial/inertial.

## Confirmed integration

The user confirmed that Sky Map catalogue-object coordinates transfer successfully into Scheduler in the running OpenAstroSuite GUI. The free-point implementation reuses that exact selected-coordinate path and needs only a short UI/HIL confirmation.

## Frozen mount contract

No direct-MC mount geometry, v9 signs, Home/Park convention, serial/Wi-Fi parity, EQDrive transport logic or mount safety policy was changed by this release.
