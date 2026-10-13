# MIB2 High Software Collector — v1.1.0-rc1

This release candidate replaces v1.0.2's command assumptions with **runtime capability tests** and collects a broader set of firmware code/configuration for AltScreen, CarPlay RGI and cross-firmware porting. It is intended for Harman MHI2/QNX 6.5 through M.I.B. It does not install hooks, patch firmware, modify coding, or remount firmware filesystems.

**Status:** Release candidate. Host/simulated-QNX tests passed. This exact script **has not yet been executed on a physical MMI** and must not be represented as car-tested.

## Install and run

Copy `mod/custom.sh` from the release ZIP to the **M.I.B. SD card** as `/mod/custom.sh`. Back up any previous custom script first. Use the M.I.B. SD card in SD1, then on the vehicle open:

`GEM → M.I.B. → Advanced Settings → Run /mod/custom.sh from m.i.b. SD`

The script prints eight numbered stages and clear command availability results to the MMI screen. Do not remove the SD until it explicitly states `SAFE TO REMOVE THE SD CARD`. On a failure, the screen shows the failed stage, the reason, and SD-removal guidance. The script never automatically reboots the car.

## Ordered workflow

1. **SD preflight.** Find a removable SD mount, test directory creation, write/read-back, rename, deletion and `sync`. If the test fails, try `mount -uw` **only for that removable SD mount**, then repeat the test. If it still fails, stop with the reason on screen. Firmware mounts are never remounted.
2. **Command audit.** List all command availability results on screen and in `capabilities.txt`. Test the actual required command options on a small file (e.g. `find -name`, `grep -i -E`, `awk tolower`, `mkdir -p`, `df -k`, copier and checksums). Stop if mandatory features are missing; use supported fallbacks or mark diminished coverage for optional missing tools. K2161 commonly lacks external `tr`, `date`, `strings`, `cksum`, `sha256sum`, and `grep -a`.
3. **System details.** Probe diagnostic command arguments before using them. Save supported `uname`, `pidin`, `netstat`, mount and filesystem outputs plus SD space information.
4. **Filesystem discovery.** Recursively inventory firmware code/configuration roots (QNX-direct or `/net/mmx`), not merely a fixed list of expected filenames. Never crawl SD, navigation-map/media collections, or ordinary user storage as an output.
5. **Filename collection.** Select executables/libraries/configuration by broad case-insensitive naming and component paths, including CarPlay, AirPlay, iAP2, `mm-ipod`, `lsd.jxe`, DSI, HMI, navigation, BAP, cluster, VC/HUD, ViewAreas, touchpad/rotary and firmware identity.
6. **Content discovery.** Search further plausible code/config files (max 16 MiB per searched file) using *only tested* `strings`/`grep` capabilities. If `strings` and `awk` work, extract and resolve direct `.so` dependency names against the complete inventory. Content-based binary matching without `strings` is best-effort and is explicitly marked as a limitation.
7. **Integrity checks.** Preserve canonical firmware-relative paths; copy to temporary SD files; validate source vs copied length; verify checksum if a tested hash tool is present; atomically rename copied files; verify every manifest-listed saved file and fail if any file could not be copied. If `df -k` is parseable, retain at least a 512 MiB SD free-space reserve.
8. **Flush and finish.** Write a truthful `COMPLETE` or `COMPLETE_WITH_LIMITATIONS` marker, sync, read back and sync again before stating it is safe to remove SD. If synchronization fails, remove the success marker where possible and write `COLLECTION_INCOMPLETE.txt`.

Progress messages appear during large file copies and after groups of files are copied or content-scanned. A long-running scan can still require patience on slow QNX flash storage.

## Outputs

A new SD directory (`MIB2_SOFTWARE_COLLECTOR_PID_...`, with collision suffix if needed) contains:

- `COLLECTION_COMPLETE.txt` (only when fully verified, including any stated limitations), or `COLLECTION_INCOMPLETE.txt`.
- `collector.log`, `collector_source.sh`, `capabilities.txt`, `limitations.txt`, `copy_errors.tsv`, `manifest.tsv`.
- `system/`: system diagnostics, identity candidates and summary.
- `discovery/`: complete filesystem path index, content hits, optional shared library dependency evidence and software identity candidates.
- `files/`: selected firmware binaries and configs preserving the source's filesystem layout.

ZIP and send the **entire generated folder** to the maintainer. Do not publish a collection without reviewing it: binaries and metadata can contain device-specific details.

## Important limitations

- No shell-only extractor can guarantee finding every relevant implementation detail on every firmware. The path index is provided for subsequent manual discovery and verification.
- On K2161, no verified stock `strings` or checksum utility was available in the October 10 test log. The collector therefore uses supported filename/content search and byte-count verification and marks that reduced assurance on the output.
- This script does **not** guarantee an MMI cannot be affected by excessive I/O or by defects in the M.I.B. launcher. Read-only refers to vehicle storage modification, not zero runtime resource usage.
- The first in-car test should use a healthy SD with sufficient space; do not run during firmware updates or other critical actions.

## Optional diagnostic environment overrides

`MIB2_OUTPUT_ROOT` can specify a supported removable mount path, and `MIB2_SCAN_ROOTS` can specify a whitespace-separated list of firmware source roots. These are for development/diagnostics; for normal car collection, leave them unset. The collector rejects an output root outside recognized removable-mount prefixes and excludes SD/USB roots from its firmware scan.
