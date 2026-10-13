# Collector v1.1.0-rc1 — offline validation

**Date:** 2026-10-13

**Source:** `mod/custom.sh`; `/bin/sh` syntax check and instrumented synthetic QNX-like tests using BusyBox command subsets. The K2161 unavailable-command set was reconstructed from on-car command reports stored in `/altscreen/documentation/V8I_HARVEST_FINDINGS_2026-10-10.md`.

## Test results

| Test | Expected behavior | Result |
|---|---|---|
| Shell parse | `sh -n` accepts script | PASS |
| Missing `tr`, `strings`, `date`, checksum tools | Capabilities shown; files collected; `COMPLETE_WITH_LIMITATIONS` | PASS |
| SD initially not writable | Attempt `mount -uw` on selected SD only, retest and continue | PASS |
| Nonexistent SD root | Stop at stage 1 with clear reason; do not begin collection | PASS |
| Missing `grep` and `awk` | Fallback to supported filename search; report reduced coverage | PASS |
| Source file copy fails | Record offending source + error, no COMPLETE marker | PASS |
| `cp` missing | Use tested `cat` fallback and compare file sizes | PASS |
| Full supported test toolchain | Optional `strings` + checksum paths, complete normally | PASS |
| `mkdir -p` option unavailable | Stop during preflight command tests with explicit reason | PASS |
| Final `sync` fails | Exit failure, remove COMPLETE marker, display sync failure and no premature safe-to-remove message | PASS |
| `find` unavailable | Stop during command testing with explicit reason | PASS |

**Coverage fixture:** synthetic K2161-like files for `/ifs/lsd.jxe`, `lsd.sh`, `bundles_dsitracer_enabled.properties`, `mm-ipod`, `ipod-drvr-iap2.so`, `vc_render`, DSI library and an otherwise generic library with embedded CarPlay text. Filename and bounded content discovery produced matching results; unrelated binary not selected. Preserved source-relative paths and verified manifest file counts.

**Not tested:** actual QNX 6.5 `/bin/sh` and BusyBox command variants on a live Audi K2161; real SD remount privileges; discovery runtime/thermal impact on the actual MMI; long-duration scan on other vendor firmware; hardware connection/disconnection. On-car approval must come after a controlled test.

**Release disposition:** Suitable for maintainer review and *candidate* pre-release only; no GitHub update or release publication has been performed. Source edits only.
