# MIB2 High Software Collector v1.2 — firmware-neutral revision

**Date:** 2026-10-14  
**Status at time of testing:** Offline release candidate; not yet tested end-to-end on physical MMI hardware.

## Changes from the original v1.2 candidate

1. Removed all `K2161` / `K3346` references from the executable script, user-facing error messages and README; version remains **v1.2**.
2. Preserved the existing eight-stage collection, focused basename-based dynamic search, read-only firmware access, exact-path manifest, large-file support, and `wc -c` / calibrated `ls -l` byte-count alternatives.
3. Replaced the model-referenced free-space check with a **filesystem-agnostic** `df -k` parser: accepts one-row tables with an Available column or validated six-field headerless output; **requires the reported mount path to match the selected removable SD**, treating `/net/mmx` as the M.I.B. access alias and ignoring a trailing slash. No hardcoded firmware train or required mount prefix is used in parsing.
4. Retained the documented physical MIB2 High external-command set. No new external commands or option flags were added. Optional unrecognized `df` layouts remain `UNKNOWN`, logged as a limitation; copy errors are fatal.

## Fresh post-revision offline regression tests (11/11 passed)

| Scenario | Result |
|---|---|
| Restricted verified-command set; `wc` present; six-field headerless df | PASS |
| Same restricted set without `wc`; calibrated `ls -l` alternative | PASS |
| `df -k` with an Available header and matching removable mount | PASS |
| Unrecognized `df` output | PASS — `UNKNOWN`, explicit limitation |
| Valid `df` output for **the wrong mount** | PASS — rejects free-space claim, explicit limitation |
| Valid `df` reporting insufficient space (<512 MiB reserve) | PASS — stops with incomplete marker |
| Mandatory `awk` missing | PASS — stops during command checks |
| `wc` missing plus unusable `ls -l` | PASS — stops during command checks |
| `cp` failure during selected firmware file copy | PASS — stops with incomplete marker |
| `lsd.jxe` of **54,701,888 bytes** | PASS — copied and length-verified |
| Fixture with 5,064 irrelevant files plus six relevant firmware files | PASS — copied six, retained full path inventory |

Tests used `/bin/sh` with an isolated PATH containing only the allowed commands, mock `df` variants and synthetic read-only firmware input trees. None of these are a substitute for an in-car test. The original v1.2 candidate previously reported 21 simulated tests; this revision has **11 newly executed** tests, not a claim that every prior test was rerun.

## Sanity audit

- `sh -n` and `bash -n`: PASS.
- No literal `K2161`, `K3346`, model names or firmware train IDs in `mod/custom.sh` or user README: PASS.
- No new external commands, no `rmdir`, `rm -r`, `tr`, `strings`, `date`, `sha256sum`, `readelf`, `objdump`, external `printf`, GNU-only `stat`/`find` flags: PASS.
- No fixed per-file size cap, no entire-file binary-content sweep: PASS.
- Selected source files copied only to approved removable output; any `mount -uw` is restricted to selected removable path: source review PASS.
- Completeness test distinguishes successful reports from incomplete error runs; copy length only, **not a cryptographic checksum**.
- Firmware-neutral means version-independent selection and reporting, **not verified compatibility with every MIB2 variant**. K2161 command evidence is retained only in the historical validation record, not encoded into the running collector.

**Remaining limitations:** The filename-based collector may omit newly named dependencies; a length match does not establish cryptographic identity; unknown `df` output cannot enforce the 512 MiB reserve and is explicitly reported; real QNX car tests remain required.
