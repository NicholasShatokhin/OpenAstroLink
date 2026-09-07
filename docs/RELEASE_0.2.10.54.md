# OpenAstroLink / OpenAstroSuite v0.2.10.54

## 2026-09-06 HIL hardening

This release is driven by real QHY5III462C + Gemini focuser + direct SynScan/EQDrive Wi-Fi HIL rather than by a new mount geometry model.

### Confirmed in HIL

- Arbitrary/free-point Sky Map GOTO physically moves the mount to the selected part of the sky.
- Sky Map catalogue/free-point coordinates can be transferred into Scheduler.
- Direct-MC v9 GOTO and Abort remain operational; v9 geometry is frozen and unchanged.
- QHY native Live View repeatedly starts/stops; still FITS captures are written successfully.
- Gemini focuser absolute motion, halt/cancel recovery and temperature/status reporting work.
- Scene autofocus cancellation restores the starting focuser position.
- SER basic compatibility remains confirmed by opening OAL SER output in AutoStakkert.

### Direct Wi-Fi manual slew reliability

The GUI log showed press/release requests reaching the node, while physical UDP/11880 motion could still ignore a press or continue after release. The direct Wi-Fi backend now:

- validates reply source address **and source port**;
- uses Motor Controller instant stop (`L`) for manual release/abort;
- retries manual start/stop transactions;
- verifies the running/stopped post-condition from live axis status;
- avoids the old 2.5-second stop-and-wait before every manual press;
- avoids stopping an already-idle second axis on single-axis presses;
- fails safe by stopping both axes when a manual start cannot be confirmed.

Axis-inversion controls in the Mount tab are now checkboxes that show the current profile state. They alter only profile signs; the v9 coordinate equations are not changed.

### Sky Map → Mount target

Selecting any catalogue or free-point Sky Map target now also copies its J2000 RA/DEC into the synchronized Mount target fields. Selection does not slew automatically; Slew remains explicit.

### Scene autofocus

HIL exposed two independent issues: sparse structured targets were over-metered up to 10 seconds, and a global whole-frame edge score was noisy enough to prefer a ~1% accidental metric change. Scene AF now:

- meters the bright tail (P99/P99.5) instead of requiring a bright global P50/P95;
- caps scene-meter acquisition at 2 seconds and four attempts;
- holds that exposure fixed for the complete focus sweep;
- evaluates robust high-frequency structure per tile and aggregates the strongest useful tiles;
- uses at most two frames per scene focus position for repeatability;
- requires an improvement larger than a 2%/measured-spread uncertainty floor before leaving an already-good starting focus;
- uses compact ±2-fine-step refinement near a local maximum instead of a broad fine sweep;
- verifies a moved-to candidate peak and restores/retains the original focus when the improvement does not repeat.

The configured focus range is still a real safety/search bound. A total range of 1600 means only ±800 steps from the starting position; it cannot find a focus several thousand steps away.

### Still-image auto-exposure

The supplied QHY FITS sequence demonstrates a two-point limit cycle: a sparse bright scene alternated near ~0.64 s and ~1.43 s because the old controller targeted the global median, then crossed a hard highlight-clipping threshold. The controller now:

- detects sparse bright-tail scenes and controls P99.5 instead of global median;
- uses smooth damping and log-space bisection on a target crossing;
- accepts small (<~0.5%) specular/star clipping when the controlled bright-tail level is healthy instead of applying a hard 0.5%-adjacent cliff;
- locks the exposure once inside the acquisition band and only unlocks after persistent scene change;
- resets its history whenever camera gain changes.

### Preview reliability

- Bright-target detection now runs on the clean camera image, not on the rendered preview after crosshair/grid overlays; this fixes false `x=17, y=17` targets caused by UI graphics.
- State refresh uses the `latest` preview for live-frame IDs, avoiding benign cache-eviction races at high Live View FPS.

## Remaining HIL gates

- Re-test Wi-Fi press/release reliability over many rapid direction changes.
- Re-test scene autofocus near true focus and from a deliberately offset but in-range position.
- Re-run still auto-exposure on the same sparse daylight target and verify it locks rather than oscillates.
- Star-field autofocus/HFR remains a separate night HIL gate.
- macOS and Raspberry Pi 5 physical qualification remain pending.
