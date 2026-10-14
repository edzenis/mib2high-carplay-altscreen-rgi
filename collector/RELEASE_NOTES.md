# MIB2 High Software Collector v1.2

The previous version was a bit too aggressive with file discovery — one test ended up collecting over 5,000 files. This release narrows things down to files useful for CarPlay, AltScreen and RGI research.

### Changes
- More selective file discovery. The full filesystem path list is still saved for reference.
- Removed the dependency on `rmdir`.
- `wc` is no longer required. Units without it can use a tested `ls -l` fallback.
- Large files such as `lsd.jxe` are copied without an arbitrary size limit.
- Improved SD free-space checks and error handling.
- Command compatibility checks are still performed before collection starts.

### Installation

Extract the ZIP to the root of your M.I.B. SD card and run **Run custom script** from the M.I.B. menu.

Back up any existing `mod/custom.sh` before replacing it. Don't remove the SD card until the script says it's safe.

### Testing

Passed offline tests using simulated K2161 and K3346 environments. **This version has not yet been tested on a physical MMI.**

If you encounter issues, please open a GitHub issue with the collector log and `capabilities.txt`, after checking them for private information.
