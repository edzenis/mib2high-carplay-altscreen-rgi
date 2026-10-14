# MIB2 High Software Collector v1.2

**Status:** Public v1.2 pre-release. Tested offline only; not yet tested on a physical MMI. Intended for compatible Harman MIB2 High (MHI2/QNX) firmware research supporting CarPlay AltScreen and route guidance (RGI). No specific software train is required or checked.

## Use

Copy/extract `mod/custom.sh` to the M.I.B. SD card as `/mod/custom.sh`, after backing up any previous custom script. From the car's M.I.B. Advanced Settings, select **Run custom script**. Read the eight stages displayed by the MMI. Never remove the card before the MMI explicitly reports it safe.

The script does not install AltScreen or RGI, modify OEM files, alter coding, or reboot. It reads firmware files and writes the results **only to an approved removable SD/USB mount**. If the SD cannot be written, the script tries `mount -uw` only on the selected removable mount and retests before proceeding. If a mandatory capability or file-copy verification fails, it prints the failed stage/reason and leaves `COLLECTION_INCOMPLETE.txt` where writable.

## Collection stages

1. **SD write test:** Write, read, rename, delete, sync (no `rmdir`).
2. **Command tests:** Test `cp`, `awk`, `find -name/-print`, `ls -l`, `mkdir -p`, calibrated byte counting (`wc -c` or `ls -l`). Test `df -k` if available. Stop on missing mandatory commands or options. Report all results on the MMI.
3. **System details:** Capture tested `uname -a` and mount information when available. Recognize validated header-based or six-field headerless `df -k` layouts only when the reported mount corresponds to the selected SD, including M.I.B. `/net/mmx` aliases.
4. **Discovery:** One read-only filename/path inventory pass through known firmware roots, excluding SD/USB roots; then select narrow filename patterns. **No binary-content scan.**
5. **Core files:** Copy exact critical CarPlay/iAP2/HMI/DSI/VC names.
6. **Related files:** Copy a narrow selection of libraries, renderer, navigation/BAP, input, and cluster/HMI configuration files. A full path index remains available for manual investigation.
7. **Verification:** Verify each copy's byte count against source, and re-check each saved file against its manifest. A failed copy causes an incomplete result.
8. **Finalize:** Write truthful completion marker, sync, read it back, sync again; only then display SAFE TO REMOVE.

## Important differences versus v1.1.0-rc1

- No `rmdir` dependency; no recursive deletion, so no unverified `rm -r` fallback.
- When `wc -c` is absent or its probe fails, v1.2 calibrates `ls -l` byte columns using two SD probe files of exactly 10 and 31 bytes, then tests the selected size method before copying.
- The script's external-command set is restricted to operations previously demonstrated on a physical MIB2 High unit: `mkdir`, `rm`, `mv`, `sync`, `cp`, `awk`, `find`, `wc`, `ls`, `df`, `mount`, `uname`. Required behavior is probed on the executing firmware. There is no `strings`, `tr`, `date`, `rmdir`, `sha256sum`, `printf`, `readelf`, `objdump`, or bundled helper binary.
- Dynamic paths are still discovered, but copying is based on carefully selected basenames and file types, **not all contents of HMI/navigation directories** or thousands of binary-content searches.
- **No file-size cap:** Large firmware binaries such as `lsd.jxe` are not rejected by arbitrary limits. With a verified `df -k` layout and matching SD mount, the collector enforces a 512 MiB free-space reserve. Unrecognized layouts are recorded as limitations rather than guessed.
- All paths copied preserve their original relative source paths under `files/`. Multiple aliases are deduplicated on the destination path.

## Generated SD directory

`MIB2_SOFTWARE_COLLECTOR_v1.2_PID_<pid>/` contains `capabilities.txt`, `collector.log`, `manifest.tsv`, `copy_errors.tsv`, `limitations.txt`, `COLLECTION_COMPLETE.txt` (or `COLLECTION_INCOMPLETE.txt`), `system/`, `discovery/`, and `files/`. `discovery/all_firmware_paths.txt` is a path **index**, not copies of all discovered files.

`COMPLETE_WITH_LIMITATIONS` is expected when copied files are verified by their lengths without a matching cryptographic hash. A byte-length match does not prove byte-for-byte identity, so the report does not claim stronger verification.

## Safety and limits

No arbitrary assumptions about other MMI firmware versions. Car testing remains necessary: offline mock shell tools do **not** prove actual QNX compatibility. Unsupported diagnostic commands are not called. A `df -k` format the script cannot interpret is reported as an explicit limitation; in that case a 512 MiB reserve is not guaranteed, and any copy error stops the run. The SD filesystem may also impose maximum file sizes independent of this script.

Firmware/configuration files can contain device identifiers and potentially sensitive data. Review contents before private sharing.

Optional diagnostic overrides (normally unset): `MIB2_OUTPUT_ROOT` accepts only known removable-mount prefixes and `MIB2_SCAN_ROOTS` accepts whitespace-separated firmware roots. Only use trusted paths; SD/USB roots are refused as sources.
