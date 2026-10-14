# MIB2 High Software Collector v1.2

**Firmware-neutral pre-release — offline-tested, not yet car-tested.**

Replaces `collector-v1.1.0-rc1` for CarPlay AltScreen and route-guidance (RGI) firmware research.

- Focused filename/path discovery instead of broad binary content scanning and 5,000+ matching files.
- Preserves a full filesystem path inventory for identifying additional dependencies without copying everything.
- Uses only external commands demonstrated on a physical MIB2 High K2161; probes required options on the running unit.
- Eliminates `rmdir` and mandatory `wc`: a calibrated `ls -l` size fallback supports units without `wc`.
- Retains SD write/read/rename/delete/sync tests, guarded SD-only remount retry, MMI-visible stages, safe failure reporting, and original-path manifests.
- Retains large file copying with no arbitrary per-file size limit (offline-tested with a 54,701,888-byte `lsd.jxe`).
- Recognizes validated header-based and headerless `df -k` formats and validates the selected removable mount before trusting free space.
- Verifies copies by file length and final manifest checks. A length match is **not** cryptographic verification.

**Validation:** 11/11 fresh firmware-neutral regression tests passed. Tests are synthetic and do not establish physical compatibility for untested firmware. See `TEST_REPORT.md` in the ZIP.

**Install:** Extract the ZIP to the M.I.B. SD-card root so that `mod/custom.sh` exists, backing up any previous `mod/custom.sh`. Run from M.I.B.'s “Run custom script” menu. Read stage/error messages on the MMI and do not remove SD until it explicitly reports safe removal.

The superseded collector v1.1.0-rc1 release is removed after the new ZIP has been uploaded and verified. The independent RGI v1.1 and AltScreen v1.0 releases are unaffected.
