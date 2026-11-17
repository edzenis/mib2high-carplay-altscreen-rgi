# AltScreen v1.1 — K2161 CarPlay Virtual Cockpit and HUD

**Status:** compiled and tested offline; not newly validated on the vehicle. **This is a separate AltScreen release from the existing HUD-only RGI `v1.1` tag.**

## Changes from AltScreen v1.0

- Reorganized K2161 input and output connectors around a firmware-independent video/navigation core.
- Virtual Cockpit switches to context **76**, with read-back-based reacquisition and pre-session restoration; context **77** was used previously.
- ViewArea is **500, 72, 440, 330** for all four views. The previous small-gauge `leftAligned` behavior is not restored; `topAligned` is used.
- Added more diagnostics, guidance data handling and trip-summary text presentation.
- USB CarPlay supplementary-stream teardown is isolated from stock stream teardown. This targets the Audi A4 B9 USB disconnect issue but **has not been confirmed fixed** on the reporter's car.
- No custom graphical maneuver renderer is included; the original H.264 decoder is retained unchanged.

## NavActiveIgnore — NOT INCLUDED

**NavActiveIgnore is NOT bundled, automatically installed, or modified by this release.** Vehicles that require it to show CarPlay maps instead of the Audi compass must install and enable a compatible NavActiveIgnore / dual-navigation modification **separately**, by the established M.I.B. procedure.

## Installation and rollback

Download `AltScreen-v1.1-install.zip`. Unzip to the root of the M.I.B. SD card and run `mod/custom.sh` through **Run custom script**. Reboot MMI after installation. Rerun to collect logs. For rollback create `mod/mib2high-carplay-rgi-v1.1/ROLLBACK` on the SD card and run the script, then reboot. Preserve backups.

Complete editable source, exact changes, helper provenance and build instructions are in `AltScreen-v1.1-source.zip` and [`altscreen/v1.1/`](../../altscreen/v1.1/).
