# AltScreen v1.1 — K2161 Virtual Cockpit CarPlay map and HUD guidance

AltScreen presents the iPhone's supplementary CarPlay map video in the Audi Virtual Cockpit while keeping the factory HUD turn-by-turn channel independent.

**Platform:** Harman MIB2 High K2161 / MU1421. This release is compiled and checked offline. AltScreen v1.0 was tested on an Audi Q7 4M; v1.1 has not been independently car-validated. Other cars and firmware are not certified.

## Important: NavActiveIgnore is NOT installed

**AltScreen v1.1 does not bundle, enable, disable, or install NavActiveIgnore.** If your car needs NavActiveIgnore to show a CarPlay map rather than the Audi compass page, install and enable the compatible NavActiveIgnore / dual navigation modification **separately**, using the vehicle's established M.I.B. procedure. Check that the modification is compatible with your firmware. The AltScreen installer leaves the existing setting untouched.

## Functionality

- 1440×542, 30 FPS supplementary H.264 CarPlay video via native displayable 58.
- Virtual Cockpit context 76, with read-back-driven context reacquisition; no custom maneuver renderer.
- Dedicated instruction-card URL advertised; `topAligned` placement and a 440×330 view area for both gauge modes.
- Independent RGI/BAP HUD maneuver and distance outputs. Formatted trip-summary text may appear in factory info output.
- Firmware-specific K2161 input/output adapters around a firmware-independent core.
- Source-only native/Java release build and guarded install, collect, rollback.

## Compatibility notes

v1.1 changes the VC presentation policy from AltScreen v1.0, including map-only context 76, safe-area geometry and available alignment URLs. The original leftAligned small-gauge layout from v1.0 is **not** restored in v1.1. A reported A4 B9 USB-disconnect `dio_manager` crash has a targeted teardown-source change, but remains **unconfirmed fixed** without a re-test.

## Installation

Extract `AltScreen-v1.1-install.zip` to the SD-card root (contains `mod/custom.sh`). Run M.I.B. **Run custom script**. Reboot the MMI when prompted. Repeat to collect logs. For rollback, create `mod/mib2high-carplay-rgi-v1.1/ROLLBACK`, run the script, then reboot. Keep the `backup` directory. Read the included installer instructions before running.

The installer does not install or change NavActiveIgnore. It modifies K2161-specific system components; use only with backups and a working recovery path.

See `CHANGES.md` and `BUILDING.md` for source-level changes and reproducibility restrictions.
