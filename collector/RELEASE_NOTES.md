# MIB2 High Software Collector v1.1.0-rc1

**Pre-release — tested offline, not on an actual MMI.**

Supersedes `collector-v1.0.2`.

- Verifies SD write/read/rename/delete/sync; retries `mount -uw` only on selected SD mount, then retests.
- Displays runtime command capability checks on the MMI and uses only tested supported options.
- Collects targeted firmware essentials plus dynamically discovered AltScreen, CarPlay, RGI, iAP2, DSI, HMI, BAP, cluster/VC/HUD, touch and navigation files for cross-firmware research.
- Preserves source paths, records inventory, reports coverage limitations, verifies copies, and stops with a clear reason on failure.
- Read-only to vehicle firmware; only the selected removable storage is written.
- Ten offline test scenarios passed, including missing QNX utilities, remount and failed copy/sync.

**Important:** This exact script has not been car-tested. Check `TEST_REPORT.md` in the ZIP for the scope and limits of validation.

### Usage

Extract the ZIP to the root of an M.I.B. SD card so the collector is at `/mod/custom.sh`. Back up any existing custom script first. Run from M.I.B.'s Advanced Settings “Run custom script”, and do not remove the card until the MMI explicitly reports it is safe.

The former `collector-v1.0.2` release is superseded and will be removed only after the new asset is uploaded and verified.
